package main

import (
	"encoding/json"
	"os"
	"path/filepath"
	"testing"
	"time"
	"pvzrelay/internal/catalogwire"
)

// Opt-in local native-app fixture. Never enabled on an ordinary test run.
func TestMatchNativeLab(t *testing.T) {
	folder:=os.Getenv("PVZ_MATCH_LAB_DIR");if folder=="" {t.Skip("manual native application fixture")}
	a,addrA:=startTestServer(t,nil);b,addrB:=startTestServer(t,nil)
	nodes:=[]catalogwire.Node{matchNode("1",addrA),matchNode("2",addrB)}
	matchConfigure(a,"1",nodes);matchConfigure(b,"2",nodes)
	// Native diagnostic controls discovery independently; gameplay/control RPC is real TCP.
	for _,s:=range []*Server{a,b} {v:=*s.operations();v.Catalog.Revision=0;s.ops.current.Store(&v)}
	os.MkdirAll(folder,0700);data,_:=json.Marshal(map[string]string{"a":addrA,"b":addrB})
	if err:=os.WriteFile(filepath.Join(folder,"ready.json"),data,0600);err!=nil{t.Fatal(err)}
	deadline:=time.Now().Add(40*time.Minute)
	for time.Now().Before(deadline) {if _,err:=os.Stat(filepath.Join(folder,"stop"));err==nil{return};time.Sleep(100*time.Millisecond)}
	t.Fatal("native lab fixture deadline exceeded")
}
