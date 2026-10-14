'use strict';
const assert = require('assert');
const path = require('path');
const fs = require('fs');
const vm = require('vm');
const {spawn} = require('child_process');
const root = path.resolve(__dirname,'..');
async function main() {
    const host = spawn(path.join(root,'.pio','preview','preview_host.exe'));
    let output = '';
    host.stdout.on('data',chunk=>output+=chunk);
    host.stdin.end('advance 12500\n');
    await new Promise((resolve,reject)=>{host.on('error',reject);host.on('exit',code=>code===0?resolve():reject(Error(`exit ${code}`)));});
    const events = output.trim().split(/\r?\n/).map(JSON.parse);
    assert(events.some(event=>event.type==='ready'));
    assert.strictEqual(events.filter(event=>event.type==='rotation').length,3);
    const writes = events.filter(event=>event.type==='write' && event.tick>=6250 && event.tick<12500);
    assert.strictEqual(writes.length,129); // 128 columns plus end blanking
    assert(writes.every(event=>Number.isInteger(event.bits))); 
    assert.strictEqual(writes[128].bits,0);
    const server = spawn(process.execPath,[path.join(__dirname,'preview_server.js')],{env:{...process.env,PREVIEW_PORT:'18080'}});
    try {
        await new Promise((resolve,reject)=>{server.on('error',reject);server.stdout.once('data',resolve);server.once('exit',()=>reject(Error('bridge failed')));});
        const page = await (await fetch('http://127.0.0.1:18080/')).text();
        const script = [...page.matchAll(/<script>([\s\S]*?)<\/script>/g)][0][1];
        new vm.Script(script);
        const elements = {}, canvas = new Proxy({}, {get:()=>()=>{}});
        let stream;
        const context = {document:{getElementById:id=>elements[id]||(elements[id]={value:'0',checked:false,getContext:()=>canvas})},
            EventSource:class {constructor(){stream=this;}},requestAnimationFrame(){},fetch,URLSearchParams};
        vm.runInNewContext(script,context);
        const position = (column,pin,strip) => vm.runInNewContext(`ledPosition(${column},${pin},${strip})`,context);
        const near = (actual,expected) => assert(Math.abs(actual-expected)<1e-9);
        near(position(0,0,0).x,210);near(position(0,0,0).y,320);
        near(position(0,7,0).x,56);near(position(0,7,0).y,320);
        near(position(0,0,1).x,430);near(position(0,0,1).y,320);
        near(position(0,7,1).x,584);
        near(position(32,0,0).x,320);near(position(32,0,0).y,210);
        for(let column=0;column<128;column++)for(let pin=0;pin<8;pin++){
            const primary=position(column,pin,0),secondary=position(column,pin,1);
            near(primary.x+secondary.x,640);near(primary.y+secondary.y,640);
        }
        elements.flip.checked=true;near(position(0,0,0).x,56);elements.flip.checked=false;
        elements.reverse.checked=true;near(position(32,0,0).y,430);elements.reverse.checked=false;
        events.forEach(event=>stream.onmessage({data:JSON.stringify(event)}));
        const completed=vm.runInNewContext('JSON.stringify(frame)',context);
        vm.runInNewContext('frame.bits[5]=123;frame.bits[64]=255',context);
        stream.onmessage({data:JSON.stringify({type:'rotation',tick:20000,period:6250,columns:128,sensorAngleDegrees:180,stripSeparationDegrees:180,clockwise:true})});
        stream.onmessage({data:JSON.stringify({type:'write',tick:20000,strip:1,bits:0})});
        assert.strictEqual(vm.runInNewContext('frame.bits[5]',context),123);
        assert.strictEqual(vm.runInNewContext('frame.bits[64]',context),0);
        assert(elements.status.textContent.includes('HAL writes'));
        assert(!page.includes('preview_data.js'));
        const abort = new AbortController();
        const response = await fetch('http://127.0.0.1:18080/events',{signal:abort.signal});
        const reader = response.body.getReader();
        const timeout = setTimeout(()=>abort.abort(),5000);
        let received='';
        while(!received.includes('"type":"write"')){
            const result=await reader.read();assert(!result.done);received+=new TextDecoder().decode(result.value);
        }
        clearTimeout(timeout);
        const changed = await fetch('http://127.0.0.1:18080/control?rpm=600&paused=true',{method:'POST'});
        assert.deepStrictEqual(await changed.json(),{paused:true,rpm:600});
        assert.strictEqual((await fetch('http://127.0.0.1:18080/control?rpm=0',{method:'POST'})).status,400);
        for (const step of ['column','frame']) {
            const response=await fetch(`http://127.0.0.1:18080/control?step=${step}`,{method:'POST'});
            assert.strictEqual((await response.json()).paused,true);
        }
        abort.abort();
        console.log('PASS: real main stdout, live SSE, controls, sensor-left origin, PA0 inner and opposing strips');
    } finally {server.kill();}
}
main().catch(error=>{console.error(error);process.exitCode=1;});