#!/bin/bash
# ==============================================================================
# Aegis-CPS RTL Verification Suite (Icarus Verilog / WSL Ubuntu)
# ==============================================================================

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
RTL_DIR="$(dirname "$SCRIPT_DIR")/rtl"
BUILD_DIR="$RTL_DIR/build"

mkdir -p "$BUILD_DIR"
cd "$RTL_DIR"

echo "========================================================"
echo "    Aegis-CPS RTL Verification Suite (iverilog / vvp)   "
echo "========================================================"
echo "Target RTL Directory: $RTL_DIR"
echo ""

run_test() {
    local name="$1"
    local top_tb="$2"
    shift 2
    local sources=("$@")

    echo "--------------------------------------------------------"
    echo "Running Test: $name"
    echo "--------------------------------------------------------"
    
    local vvp_out="$BUILD_DIR/${name}.vvp"
    local vcd_out="$RTL_DIR/${name}.vcd"
    
    # Compile
    iverilog -o "$vvp_out" "${sources[@]}" "$top_tb"
    echo "  [COMPILE] OK -> $vvp_out"
    
    # Run
    vvp "$vvp_out"
    echo "  [SIMULATION] Finished. Waveform: $vcd_out"
    echo ""
}

# 1. Bit Timing Logic
run_test "can_btl" "tb/tb_can_btl.v" "src/can_btl.v"

# 2. Hardware Bit De-Stuffer
run_test "can_destuffer" "tb/tb_can_destuffer.v" "src/can_destuffer.v"

# 3. CAN ID Deserializer
run_test "can_id_deserializer" "tb/tb_can_id_deserializer.v" "src/can_id_deserializer.v"

# 4. BRAM Whitelist & Extended CAM
run_test "bram_whitelist" "tb/tb_bram_whitelist.v" "src/bram_whitelist.v"

# 5. Kill Wire Controller
run_test "kill_wire_ctrl" "tb/tb_kill_wire_ctrl.v" "src/kill_wire_ctrl.v"

# 6. Top-Level Integration (Full Pipeline)
run_test "aegis_top" "tb/tb_aegis_top.v" \
    "src/can_btl.v" \
    "src/can_destuffer.v" \
    "src/can_id_deserializer.v" \
    "src/bram_whitelist.v" \
    "src/kill_wire_ctrl.v" \
    "src/violation_capture.v" \
    "src/spi_dma_tx.v" \
    "src/aegis_top.v"

run_test "can_attack_regression" "tb/tb_can_attack_regression.v" src/*.v

echo "========================================================"
echo "    All Testbench Compilations & Runs Completed!        "
echo "========================================================"
