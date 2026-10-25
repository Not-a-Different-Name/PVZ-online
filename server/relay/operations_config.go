package main

import (
	"bytes"
	"crypto/ed25519"
	"encoding/base64"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"os"
	"path/filepath"
	"pvzrelay/internal/catalogwire"
	"sync"
	"sync/atomic"
	"time"
)

// Public identity only. The maintenance tool generates the private key outside
// product sources. Release preparation replaces this anchor once, before build.
const defaultCatalogPublicKey = "EyJZjxD87BdiiDvFnXKBuvdOZKjU0vt3WdZ3UaCW3QA="

type operationsConfig struct {
	Revision         uint64  `json:"revision"`
	BandwidthMbps    float64 `json:"bandwidth_mbps"`
	SendBudgetMbps   float64 `json:"send_budget_mbps"`
	Weight           uint16  `json:"weight"`
	AcceptNewRooms   bool    `json:"accept_new_rooms"`
	BusyCPUPercent   float64 `json:"busy_cpu_percent"`
	PunchEnable      bool    `json:"punch_enable"`
	PunchUDPPort     int     `json:"punch_udp_port"`
	CatalogPath      string  `json:"catalog_path"`
	CatalogPublicKey string  `json:"catalog_public_key"`
}
type operationsSnapshot struct {
	Config      operationsConfig
	LoadedAt    time.Time
	Catalog     catalogwire.Document
	CatalogBlob []byte
}
type operationsState struct {
	current     atomic.Pointer[operationsSnapshot]
	mu          sync.Mutex
	path        string
	lastContent []byte
	lastError   string
	once        sync.Once
	stopOnce    sync.Once
	stop        chan struct{}
}

func defaultOperationsConfig() operationsConfig {
	return operationsConfig{Revision: 1, Weight: 100, AcceptNewRooms: true, BusyCPUPercent: 85, PunchEnable: true, PunchUDPPort: punchUDPPortDefault, CatalogPublicKey: defaultCatalogPublicKey}
}
func (s *Server) operations() *operationsSnapshot {
	if v := s.ops.current.Load(); v != nil {
		return v
	}
	return &operationsSnapshot{Config: defaultOperationsConfig()}
}
func (s *Server) configError() string {
	s.ops.mu.Lock()
	defer s.ops.mu.Unlock()
	return s.ops.lastError
}

func (s *Server) reloadOperations(path string) error {
	s.ops.mu.Lock()
	defer s.ops.mu.Unlock()
	file, err := os.Open(path)
	if err != nil {
		s.ops.lastError = err.Error()
		return err
	}
	defer file.Close()
	data, err := io.ReadAll(io.LimitReader(file, 16385))
	if err != nil {
		s.ops.lastError = err.Error()
		return err
	}
	if len(data) > 16384 {
		err = errors.New("config exceeds 16 KiB")
		s.ops.lastError = err.Error()
		return err
	}
	cfg := defaultOperationsConfig()
	decoder := json.NewDecoder(bytes.NewReader(data))
	decoder.DisallowUnknownFields()
	if err = decoder.Decode(&cfg); err == nil {
		var extra any
		if decoder.Decode(&extra) != io.EOF {
			err = errors.New("trailing config data")
		}
	}
	if err == nil && (cfg.Revision == 0 || cfg.Weight == 0 || cfg.Weight > 1000 || cfg.BandwidthMbps < 0 || cfg.BandwidthMbps > 100000 || cfg.SendBudgetMbps < 0 || cfg.SendBudgetMbps > 100000 || cfg.BandwidthMbps > 0 && cfg.SendBudgetMbps > cfg.BandwidthMbps || cfg.BusyCPUPercent < 1 || cfg.BusyCPUPercent > 100 || cfg.PunchUDPPort < 1 || cfg.PunchUDPPort > 65535) {
		err = errors.New("invalid operations config values")
	}
	previous := s.ops.current.Load()
	if err == nil && previous != nil {
		if cfg.Revision < previous.Config.Revision {
			err = errors.New("config revision rollback")
		}
		if cfg.Revision == previous.Config.Revision && cfg != previous.Config {
			err = errors.New("config changed without increasing revision")
		}
	}
	next := &operationsSnapshot{Config: cfg, LoadedAt: time.Now()}
	if err == nil && cfg.CatalogPath != "" {
		pub, keyErr := base64.StdEncoding.DecodeString(cfg.CatalogPublicKey)
		if keyErr != nil || len(pub) != ed25519.PublicKeySize {
			err = errors.New("invalid catalog public key")
		} else {
			target := cfg.CatalogPath
			if !filepath.IsAbs(target) {
				target = filepath.Join(filepath.Dir(path), target)
			}
			var blob []byte
			blob, err = readBoundedFile(target, catalogwire.MaxBytes)
			if err == nil {
				next.Catalog, err = catalogwire.Parse(blob, pub, time.Now(), false)
				next.CatalogBlob = blob
			}
			if err == nil && previous != nil && len(previous.CatalogBlob) > 0 {
				err = catalogwire.Transition(previous.Catalog, next.Catalog)
			}
		}
	}
	if err == nil && previous != nil && len(previous.CatalogBlob) > 0 && len(next.CatalogBlob) == 0 {
		err = errors.New("cannot remove deployed catalog; retire nodes with flags")
	}
	if err != nil {
		s.ops.lastError = err.Error()
		return err
	}
	if previous != nil && cfg == previous.Config && bytes.Equal(previous.CatalogBlob, next.CatalogBlob) {
		s.ops.lastError = ""
		return nil
	}
	// Order maintenance admission with room creation under the same room lock.
	// When reload returns, no create admitted by the previous switch is pending.
	s.mu.Lock()
	s.ops.current.Store(next)
	s.mu.Unlock()
	s.ops.path = path
	s.ops.lastContent = append([]byte(nil), data...)
	s.ops.lastError = ""
	s.logf("operations config revision %d loaded; catalog revision %d", cfg.Revision, next.Catalog.Revision)
	return nil
}
func readBoundedFile(path string, limit int) ([]byte, error) {
	f, err := os.Open(path)
	if err != nil {
		return nil, err
	}
	defer f.Close()
	b, err := io.ReadAll(io.LimitReader(f, int64(limit+1)))
	if err == nil && len(b) > limit {
		err = fmt.Errorf("file exceeds %d bytes", limit)
	}
	return b, err
}

func (s *Server) startSampling(configPath string) {
	s.ops.once.Do(func() {
		s.ops.stop = make(chan struct{})
		go func() {
			ticker := time.NewTicker(time.Second)
			defer ticker.Stop()
			var fingerprint string
			for {
				select {
				case now := <-ticker.C:
					if configPath != "" {
						// Poll only bounded files. Catalog may be atomically replaced independently
						// of the config, so both paths participate in the change fingerprint.
						data, err := readBoundedFile(configPath, 16384)
						mark := string(data)
						if err != nil {
							mark = err.Error()
						} else {
							var cfg operationsConfig
							if json.Unmarshal(data, &cfg) == nil && cfg.CatalogPath != "" {
								target := cfg.CatalogPath
								if !filepath.IsAbs(target) {
									target = filepath.Join(filepath.Dir(configPath), target)
								}
								if blob, e := readBoundedFile(target, catalogwire.MaxBytes); e == nil {
									mark += "|" + string(blob)
								} else {
									mark += e.Error()
								}
							}
						}
						if mark != fingerprint {
							fingerprint = mark
							if err := s.reloadOperations(configPath); err != nil {
								s.logf("config reload rejected: %v", err)
							}
						}
					}
					s.sampleStatus(now)
				case <-s.ops.stop:
					return
				}
			}
		}()
	})
}
func (s *Server) stopSampling() {
	s.ops.stopOnce.Do(func() {
		if s.ops.stop != nil {
			close(s.ops.stop)
		}
	})
}
