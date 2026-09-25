# -----------------------------------------------------------------------------
# Synopsys Design Constraints (.sdc) for TimeQuest Timing Analyzer
# Target Clock: 50 MHz (20.0 ns period)
# -----------------------------------------------------------------------------

# Primary 50 MHz system clock from Pin 17
create_clock -name {clk} -period 20.000 -waveform {0.000 10.000} [get_ports {clk}]

# Derive clock uncertainty for Cyclone II
derive_clock_uncertainty

# Input constraints (CAN_RX from transceiver, max 5ns board delay)
set_input_delay -clock {clk} -max 5.000 [get_ports {can_rx rst_n}]
set_input_delay -clock {clk} -min 1.000 [get_ports {can_rx rst_n}]

# Output constraints (CAN_TX to transceiver and SPI to ESP32-S3)
set_output_delay -clock {clk} -max 5.000 [get_ports {can_tx spi_clk spi_mosi spi_cs_n irq_out led_*}]
set_output_delay -clock {clk} -min 1.000 [get_ports {can_tx spi_clk spi_mosi spi_cs_n irq_out led_*}]
