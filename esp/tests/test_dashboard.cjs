const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const assert = require('node:assert/strict');
const src = fs.readFileSync(path.join(__dirname,'../main/web_dashboard.c'),'utf8');
const literals = src.split('static const char DASHBOARD_HTML[] =\n')[1].split(';\n\nstatic esp_err_t')[0];
const html = literals.split('\n').map(line => JSON.parse(line)).join('');
const script = html.match(/<script>([\s\S]*?)<\/script>/)[1];
const nodes = new Map();
const get = id => { if(!nodes.has(id)) nodes.set(id,{textContent:''}); return nodes.get(id); };
let fail = false;
let status = {psram_free:0, uptime:12, fpga_reports:17, rule_reports:2, spi_ready:false,
  can_ready:false, log_ready:false, logged:5, log_errors:3, spi_rejected:4, spi_dropped:1, ip:'0.0.0.0'};
const ctx = vm.createContext({document:{getElementById:get},location:{host:'test'},
  WebSocket: class {}, setInterval(){}, setTimeout(){},
  fetch:async()=>{ if(fail) throw Error('offline'); return {ok:true,json:async()=>status}; }});
vm.runInContext(script,ctx);
(async()=>{
  await ctx.updateStatus();
  assert.equal(get('stat-psram').textContent,'0 KB');
  assert.equal(get('stat-count').textContent,17);
  assert.equal(get('spi-state').textContent,'UNAVAILABLE');
  assert.match(get('health').textContent,/Log: unavailable/);
  status.spi_ready=true; await ctx.updateStatus();
  assert.equal(get('spi-state').textContent,'INITIALIZED');
  fail=true; await ctx.updateStatus();
  assert.equal(get('spi-state').textContent,'UNKNOWN');
  assert.match(get('online').textContent,/stale/);
  assert(!html.includes('INTERDICTED &lt;40ns'));
  console.log('PASS: zero PSRAM, server counts, live service state and offline dashboard');
})().catch(e=>{console.error(e);process.exitCode=1;});
