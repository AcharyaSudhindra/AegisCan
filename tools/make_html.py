import base64

img_path = r'C:/Users/sudhi/.gemini/antigravity/brain/d850aeb7-43cc-4ba9-aab3-20c7fddf4186/fpga_pin_guide.jpg'
with open(img_path, 'rb') as f:
    b64_data = base64.b64encode(f.read()).decode('utf-8')

html_content = f'''<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <script src="https://www.gstatic.com/antigravity/web/dev/tailwindcss.min.js"></script>
</head>
<body class="bg-slate-950 text-white p-4 font-sans antialiased">
  <div class="max-w-4xl mx-auto bg-slate-900 border border-cyan-500/40 rounded-2xl p-5 shadow-2xl">
    <div class="flex items-center justify-between mb-4 border-b border-slate-800 pb-3">
      <div>
        <h1 class="text-xl font-bold text-cyan-400">Altera DE2: Exact 6-Pin Wiring Guide</h1>
        <p class="text-slate-400 text-sm">Only touch these exact 6 pins inside connector JP1 (first 3 pairs at the bottom)</p>
      </div>
      <span class="px-3 py-1 bg-cyan-950 border border-cyan-600/50 text-cyan-300 text-xs font-mono rounded-full">JP1 Header (GPIO 0)</span>
    </div>

    <!-- Annotated Image with Zero-Fail Base64 -->
    <div class="relative rounded-xl overflow-hidden border border-slate-700 mb-6 bg-black flex justify-center">
      <img src="data:image/jpeg;base64,{b64_data}" alt="FPGA Pin Guide" class="w-full max-w-3xl object-contain" />
    </div>

    <!-- Clear Wire-by-Wire Card Grid -->
    <div class="grid grid-cols-1 md:grid-cols-2 gap-3 text-sm">
      <div class="flex items-center gap-3 p-3 bg-slate-800/80 rounded-xl border border-yellow-500/40">
        <div class="w-5 h-5 rounded-full bg-yellow-400 flex-shrink-0 shadow-[0_0_10px_rgba(250,204,21,0.8)]"></div>
        <div>
          <span class="font-mono font-bold text-yellow-300 text-xs">ROW 1 RIGHT (PIN 1)</span>
          <div class="font-semibold text-white">CAN_RX &rarr; Level Shifter LV1</div>
        </div>
      </div>

      <div class="flex items-center gap-3 p-3 bg-slate-800/80 rounded-xl border border-emerald-500/40">
        <div class="w-5 h-5 rounded-full bg-emerald-400 flex-shrink-0 shadow-[0_0_10px_rgba(52,211,153,0.8)]"></div>
        <div>
          <span class="font-mono font-bold text-emerald-300 text-xs">ROW 1 LEFT (PIN 2)</span>
          <div class="font-semibold text-white">CAN_TX &rarr; MCP2515 #1 TJA1050 Pin 1</div>
        </div>
      </div>

      <div class="flex items-center gap-3 p-3 bg-slate-800/80 rounded-xl border border-cyan-500/40">
        <div class="w-5 h-5 rounded-full bg-cyan-400 flex-shrink-0 shadow-[0_0_10px_rgba(34,211,238,0.8)]"></div>
        <div>
          <span class="font-mono font-bold text-cyan-300 text-xs">ROW 2 RIGHT (PIN 3)</span>
          <div class="font-semibold text-white">SPI_CLK &rarr; ESP32 GPIO 12</div>
        </div>
      </div>

      <div class="flex items-center gap-3 p-3 bg-slate-800/80 rounded-xl border border-pink-500/40">
        <div class="w-5 h-5 rounded-full bg-pink-400 flex-shrink-0 shadow-[0_0_10px_rgba(244,114,182,0.8)]"></div>
        <div>
          <span class="font-mono font-bold text-pink-300 text-xs">ROW 2 LEFT (PIN 4)</span>
          <div class="font-semibold text-white">SPI_MOSI &rarr; ESP32 GPIO 11</div>
        </div>
      </div>

      <div class="flex items-center gap-3 p-3 bg-slate-800/80 rounded-xl border border-orange-500/40">
        <div class="w-5 h-5 rounded-full bg-orange-400 flex-shrink-0 shadow-[0_0_10px_rgba(251,146,60,0.8)]"></div>
        <div>
          <span class="font-mono font-bold text-orange-300 text-xs">ROW 3 RIGHT (PIN 5)</span>
          <div class="font-semibold text-white">SPI_CS &rarr; ESP32 GPIO 10</div>
        </div>
      </div>

      <div class="flex items-center gap-3 p-3 bg-slate-800/80 rounded-xl border border-purple-500/40">
        <div class="w-5 h-5 rounded-full bg-purple-400 flex-shrink-0 shadow-[0_0_10px_rgba(192,132,252,0.8)]"></div>
        <div>
          <span class="font-mono font-bold text-purple-300 text-xs">ROW 3 LEFT (PIN 6)</span>
          <div class="font-semibold text-white">IRQ_OUT &rarr; ESP32 GPIO 9</div>
        </div>
      </div>
    </div>
  </div>
</body>
</html>'''

out_file = r'C:/Users/sudhi/.gemini/antigravity/brain/d850aeb7-43cc-4ba9-aab3-20c7fddf4186/pin_guide.html'
with open(out_file, 'w', encoding='utf-8') as f:
    f.write(html_content)

print('pin_guide.html written successfully!')
