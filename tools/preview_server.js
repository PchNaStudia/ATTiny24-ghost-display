'use strict';
const http = require('http');
const fs = require('fs');
const path = require('path');
const readline = require('readline');
const {spawn} = require('child_process');
const root = path.resolve(__dirname, '..');
const port = Number(process.env.PREVIEW_PORT || 8080);
const child = spawn(path.join(root, '.pio', 'preview', 'preview_host.exe'), [], {stdio:['pipe','pipe','inherit']});
const clients = new Set();
let paused = false, rpm = 1200, alive = true;
const lines = readline.createInterface({input:child.stdout});
lines.on('line', line => {
    try { JSON.parse(line); } catch { console.error('Invalid host output:', line); return; }
    if (process.env.PREVIEW_TRACE === '1') console.log(line);
    for (const client of clients) {
        if (!client.write(`data: ${line}\n\n`)) client.destroy();
    }
});
const server = http.createServer((req,res) => {
    const url = new URL(req.url, 'http://localhost');
    if (url.pathname === '/events' && req.method === 'GET') {
        res.writeHead(200, {'Content-Type':'text/event-stream','Cache-Control':'no-cache','Connection':'keep-alive'});
        res.write(': connected\n\n'); clients.add(res);
        req.on('close',()=>clients.delete(res)); return;
    }
    if (url.pathname === '/control' && req.method === 'POST') {
        if (!alive) { res.writeHead(503); res.end('Host stopped'); return; }
        const step = url.searchParams.get('step');
        if (step !== null) {
            if (!['column','frame'].includes(step)) {res.writeHead(400);res.end('Invalid step');return;}
            paused = true; child.stdin.write(`${step} 1\n`);
        }
        const requested = url.searchParams.get('rpm');
        if (requested !== null) {
            const value = Number(requested);
            if (!Number.isFinite(value) || value < 120 || value > 3600) {res.writeHead(400);res.end('RPM must be 120–3600');return;}
            rpm = value; child.stdin.write(`period ${Math.round(7500000/rpm)}\n`);
        }
        if (url.searchParams.has('paused')) paused = url.searchParams.get('paused') === 'true';
        res.writeHead(200, {'Content-Type':'application/json'});res.end(JSON.stringify({paused,rpm}));return;
    }
    if (url.pathname === '/' && req.method === 'GET') {
        res.writeHead(200, {'Content-Type':'text/html; charset=utf-8'});
        res.end(fs.readFileSync(path.join(root,'display_preview.html')));return;
    }
    res.writeHead(404);res.end('Not found');
});
const timer = setInterval(()=>{
    if (alive && !paused && clients.size && child.stdin.writableLength < 4096) child.stdin.write('advance 2500\n');
},20);
child.stdin.on('error',error=>console.error('Host input:',error.message));
function stopped(message) {
    alive = false;clearInterval(timer);console.error(message);
    for(const client of clients) client.end();
    server.close();
}
child.on('error',error=>stopped(error.message));
child.on('exit',code=>stopped(`C++ host exited (${code})`));
server.on('error',error=>{console.error(error.message);child.kill();});
server.listen(port,'127.0.0.1',()=>console.log(`Live firmware preview: http://127.0.0.1:${port} (Ctrl+C to stop)`));
for(const signal of ['SIGINT','SIGTERM'])process.on(signal,()=>{clearInterval(timer);child.kill();server.close();});