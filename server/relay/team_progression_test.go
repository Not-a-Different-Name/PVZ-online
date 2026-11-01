package main

import (
 "bytes"
 "encoding/binary"
 "fmt"
 "testing"
 "time"
)

// Real loopback relay forwarding, with the current round envelope and original bytes.
func TestTeamProgressionAndSpeciesPayloadForwarding(t *testing.T) {
 for _, tc := range []struct{typ uint16; size int}{{28,50},{28,221},{28,119},{28,229},{28,185},{5,62},{5,63},{23,42},{23,106}} {
  t.Run(fmt.Sprintf("type%d_size%d",tc.typ,tc.size),func(t *testing.T){
   s,_,code,h,g:=flowPair(t);flowBegin(t,s,code,h,g)
   body:=bytes.Repeat([]byte{0xa5},tc.size);body[0],body[1]=1,2
   h.send(msgCliFlowGame,flowGameBytes(1,1,tc.typ,body))
   for i:=0;i<30;i++ {f:=g.next(time.Second)
    if f.typ==msgSrvFlowGame && len(f.payload)>=10 && binary.LittleEndian.Uint16(f.payload[8:])==tc.typ {
     if !bytes.Equal(f.payload[10:],body) {t.Fatal("relay modified team/species payload")};return
    }
   };t.Fatal("current-round team/species message not delivered")
  })
 }
}

func TestWaitingSeatTeamProgressionForwarding(t *testing.T) {
 s,_,code,h,g:=flowPair(t);flowBegin(t,s,code,h,g)
 s.mu.Lock();s.rooms[code].roundWaitMask|=1<<2;s.mu.Unlock()
 body:=bytes.Repeat([]byte{0x5a},50);body[0],body[1]=2,1
 g.send(msgCliFlowGame,flowGameBytes(1,1,28,body))
 for i:=0;i<30;i++ { f:=h.next(time.Second)
  if f.typ==msgSrvFlowGame && len(f.payload)>=10 && binary.LittleEndian.Uint16(f.payload[8:])==28 {
   if !bytes.Equal(f.payload[10:],body){t.Fatal("waiting seat team state changed")};return
  }
 };t.Fatal("waiting seat cannot synchronize its noncombat team state")
}
