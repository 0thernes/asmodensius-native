
import assert from "node:assert/strict";
import {readFileSync,mkdirSync} from "node:fs";
import {spawnSync} from "node:child_process";
import {TYPE,VERSION,FRAME_BYTES,HEADER_BYTES,RECORD_BYTES,SHAPE_BYTES,decodeFrame,decodeCreatures,decodeStatic,decodeFields,decodeJSON,decodeShapes,
  mergePrev,wrapDelta16,uidAt,findUid,orbitCamera,projectPoint,quantile} from "../../demo/viewer_core.js";

const abi=readFileSync(new URL("../../tools/az_stream.h",import.meta.url),"ascii");
for(const [name,value] of Object.entries({VERSION,FRAME_BYTES,HEADER_BYTES,RECORD_BYTES,SHAPE_BYTES,...TYPE})){
  const m=new RegExp("AZST_"+name+"\\s*=\\s*(\\d+)").exec(abi);
  assert.equal(Number(m?.[1]),value,name);
}
function frame(type,n){
  const raw=new ArrayBuffer(16+n),view=new DataView(raw);
  view.setUint32(0,0x54535a41,true);view.setUint16(4,type,true);view.setUint16(6,1,true);view.setUint32(8,n,true);
  return decodeFrame(raw);
}
function sample(count,uid,position){
  const f=frame(TYPE.CREATURES,64+count*24),v=f.payload;
  v.setUint32(24,count,true);v.setUint32(28,24,true);
  for(let i=0;i<count;i++){
    const off=64+i*24;v.setBigUint64(off,uid(i),true);
    const x=position(i);for(let k=0;k<3;k++)v.setUint16(off+8+2*k,x+k,true);
  }
  return decodeCreatures(f);
}
assert.throws(()=>decodeFrame(new ArrayBuffer(15)));
assert.throws(()=>decodeFrame(new ArrayBuffer(16)));
const m=frame(TYPE.CREATURES,64);m.payload.setUint32(24,1,true);m.payload.setUint32(28,24,true);assert.throws(()=>decodeCreatures(m));
assert.equal(wrapDelta16(65532,3),7);assert.equal(wrapDelta16(3,65532),-7);
const a=sample(4,i=>[1n,2n,0xffffffffn,0x100000001n][i],i=>i+15);
const b=sample(4,i=>[1n,5n,0xffffffffn,0x200000001n][i],i=>i+60);
assert.deepEqual([...mergePrev(a,b)],[15,16,17,0,61,62,63,1,17,18,19,0,63,64,65,1]);
assert.deepEqual([...mergePrev(b,sample(0,i=>0n,i=>0))],[]);
assert.equal(uidAt(a,3),"0000000100000001");assert.equal(findUid(a,uidAt(a,3)),3);assert.equal(findUid(a,"0000000200000001"),-1);
let seed=117;const rng=()=>{seed=(1664525*seed+1013904223)>>>0;return seed/4294967296;};
for(let j=0;j<50;j++){
  const base=Array.from({length:350},(_,i)=>(1n<<40n)+BigInt(i)),left=base.filter(()=>rng()<.65),right=base.filter(()=>rng()<.65);
  const x=sample(left.length,i=>left[i],i=>i+5),y=sample(right.length,i=>right[i],i=>1000+i);
  const past=new Map(left.map((u,i)=>[u,i+5])),res=mergePrev(x,y);
  for(let i=0;i<right.length;i++){assert.equal(res[4*i],past.get(right[i])??1000+i);assert.equal(res[4*i+3],past.has(right[i])?0:1);}
}
const large=sample(250000,i=>BigInt(i*2),i=>i),other=sample(250000,i=>BigInt(i*3),i=>i+1);
mergePrev(large,other);
const timings=[];for(let j=0;j<7;j++){const start=performance.now();mergePrev(large,other);timings.push(performance.now()-start);}
console.log("250k UID merge median ms",quantile(timings,.5).toFixed(2));assert.ok(quantile(timings,.5)<30,"250k merge exceeded median 30 ms");
for(let i=0;i<20;i++){
  const w=64+i,h=42+i,z=32,box=[w,h,z],yaw=rng()*Math.PI*2,pitch=(rng()-.5)*2.5,d=80+rng()*120;
  const c=orbitCamera(box,yaw,pitch,d,1280,720),target=box.map(q=>q/2);
  const expected=[Math.cos(yaw)*Math.cos(pitch),Math.sin(yaw)*Math.cos(pitch),Math.sin(pitch)];
  for(let k=0;k<3;k++)assert.ok(Math.abs(c.eye[k]-(target[k]+d*expected[k]))<1e-4);
  const pix=projectPoint(c,target);assert.ok(Math.abs(pix[0]-640)<1e-4&&Math.abs(pix[1]-360)<1e-4);
  const fit=orbitCamera(box,yaw,pitch,0,1280,720);
  for(let corner=0;corner<8;corner++){const q=projectPoint(fit,box.map((v,k)=>(corner>>k&1)*v));assert.ok(q[0]>=0&&q[0]<=1280&&q[1]>=0&&q[1]<=720&&q[2]>0);}
}
if(process.argv.length>=4){
  const [live,dir]=process.argv.slice(2);mkdirSync(dir,{recursive:true});
  const run=spawnSync(live,["--dir",dir,"--stream","--lab","--seed","7","--n","200","--cap","1024","--grid","4,4,3","--max-ticks","5","--threads","2"],{input:undefined,stdio:["ignore","pipe","pipe"],maxBuffer:64*1024*1024,timeout:60000});
  assert.equal(run.status,0,run.stderr?.toString()||run.error?.message);
  const seen=new Set();let last=null;
  for(let off=0;off<run.stdout.length;){
    const size=run.stdout.readUInt32LE(off+8),bytes=run.stdout.subarray(off,off+16+size);
    const f=decodeFrame(bytes.buffer.slice(bytes.byteOffset,bytes.byteOffset+bytes.byteLength));seen.add(f.type);
    if(f.type===TYPE.CREATURES){const s=decodeCreatures(f);assert.equal(s.before,s.after);assert.ok(s.count>0);last=s;}
    else if(f.type===TYPE.STATIC)assert.ok(decodeStatic(f).heights.length);
    else if(f.type===TYPE.FIELDS)assert.ok(decodeFields(f).water.length);
    else if(f.type===TYPE.SHAPES)assert.ok(decodeShapes(f).length);
    else if(f.type===TYPE.JSON)assert.ok(typeof decodeJSON(f)==="object");
    off+=bytes.byteLength;
  }
  assert.deepEqual([...seen].sort(),[1,2,3,4,5]);assert.ok(last);console.log("Decoded native five-tick stream");
}
console.log("PASS viewer core");

if(process.argv[4]){
  const probe=process.argv[4];
  for(let i=0;i<20;i++){
    const box=[64,32,16],yaw=(rng()-.5)*6,pitch=(rng()-.5)*2.7,distance=i%2?0:30+rng()*100;
    const target=i%3?box.map(v=>v*.5):[14.3,13.1,5.2],width=1251,height=737;
    const c=orbitCamera(box,yaw,pitch,distance,width,height,target);
    const run=spawnSync(probe,[...box,width,height,yaw,pitch,distance,...target].map(String),{encoding:"utf8",timeout:3000});
    assert.equal(run.status,0,run.stderr);
    const actual=JSON.parse(run.stdout);
    for(const key of ["eye","right","up","forward"])for(let k=0;k<3;k++)assert.ok(Math.abs(actual[key][k]-c[key][k])<1e-4,`${key}[${k}] mismatch at pose ${i}`);
    assert.ok(Math.abs(actual.focal-c.focal)<2e-4,"native focal length mismatch");
  }
  console.log("20 native camera poses matched the actual renderer");
}
await import("./viewer_transport_test.mjs");
