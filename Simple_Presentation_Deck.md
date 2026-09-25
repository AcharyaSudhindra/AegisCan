# 🎯 Aegis-CPS: Simple 7-Slide Pitch Deck (Plain English)

> **Keep it simple:** Hackathon judges only give you 3 to 5 minutes. Don't read complex paragraphs. Put 3 bullet points on each slide and speak naturally using the simple scripts below.

---

### SLIDE 1: The Big Idea
* **Headline:** Aegis-CPS: A Nanosecond Hardware Firewall for Smart Cars
* **The Problem:** Modern cars can be hacked through the radio or OBD port.
* **Our Solution:** A tiny microchip firewall that kills cyberattacks in **under 180 nanoseconds**—before they reach the engine or brakes.
* **Team:** Sudhindra M Acharya (VLSI & Embedded Systems)

> **What to Say:**  
> *"Every car on the road today runs on a network called CAN, which has zero passwords or encryption. We built Aegis-CPS: a hardware firewall that stops vehicle hackers in less than 180 nanoseconds—right on the physical wire."*

---

### SLIDE 2: The Danger (How Cars Get Hacked)
* Cars have dozens of mini-computers (ECUs) talking on a shared 2-wire bus.
* **The Flaw:** Any device on the bus can pretend to be the brakes, steering, or engine.
* If a hacker breaches the infotainment Wi-Fi or OBD-II port, they can inject fake commands (like *"Accelerate to Max"* or *"Disable Brakes"*).

> **What to Say:**  
> *"The CAN bus is like a room where everyone speaks through a megaphone. If an attacker enters the room, they can shout fake commands, and the car's engine will blindly obey them because it has no way to check who is speaking."*

---

### SLIDE 3: Why Software Firewalls Fail
* **The "Too Late" Problem:** Software firewalls have to read the whole message before deciding if it's dangerous.
* Reading and checking a message in software takes **1 to 5 milliseconds**.
* **The Reality:** By the time software realizes an attack is happening, the brake or engine has already reacted.

> **What to Say:**  
> *"Software firewalls are like security guards who check your ID only after you've already walked past them. By the time software processes the attack, the message has already been received by the engine."*

---

### SLIDE 4: Our Innovation — The "Silicon Sentry"
* Instead of waiting for the full message, our **FPGA** checks every single bit in real time.
* It checks the Sender ID in **80 nanoseconds** using a built-in hardware whitelist.
* **The Kill Wire:** If the ID is fake, the FPGA immediately jams the wire with dominant bits, destroying the message mid-air.
* **Result:** The fake message gets corrupted on the wire; the engine never sees it!

> **What to Say:**  
> *"We moved security down from software into pure silicon. Our FPGA evaluates the message bit-by-bit. The moment an unauthorized ID appears, the FPGA pulls a 'Kill Wire' to destroy the packet mid-transmission in under 180 nanoseconds."*

---

### SLIDE 5: How It Works (The 2-Brain Architecture)
* **Brain 1 (Altera Cyclone II FPGA):**
  * The ultra-fast muscle.
  * Checks bits at 50 MHz clock speed and triggers the physical Kill Wire.
* **Brain 2 (ESP32-S3 Microcontroller):**
  * The smart recorder.
  * Receives attack reports from the FPGA via high-speed SPI.
  * Encrypts the incident using AES-256 and shows it live on a web dashboard.

> **What to Say:**  
> *"We use two brains: The FPGA is the muscle that kills the attack in nanoseconds. The ESP32 is the brain that records what happened, encrypts the evidence, and alerts the driver on a dashboard."*

---

### SLIDE 6: Our Live Demo Setup
* **1. Attacker (ESP32-S3):** Sends real wireless attacks from a phone (e.g., *"Spoof RPM to Max"*).
* **2. Victim ECU (Arduino):** Acts as the car's dashboard/engine.
* **3. Aegis-CPS (FPGA + ESP32):** Intercepts the attack on the CAN bus.
* **Proof:** A USB Logic Analyzer shows the exact square waves on screen where the FPGA forcefully crushes the attacker's signal.

> **What to Say:**  
> *"Here on the table, we have a complete miniature vehicle network. Using this phone, I'll send a wireless spoofing attack. Look at our logic analyzer: the red line is our FPGA killing the attacker's frame before it can even finish sending."*

---

### SLIDE 7: Market Impact & Why This Wins
* **Tiny & Cheap:** Our design uses only **1% of the FPGA** (~3,500 logic gates). It can be built directly into standard ₹15 ($0.15) CAN chips.
* **Zero Disruption:** Plugs into existing cars with zero rewiring.
* **Zero Delay:** Adds 0 microseconds of lag to normal vehicle traffic.
* **Markets:** Electric vehicles, self-driving cars, industrial robots, and aerospace drones.

> **What to Say:**  
> *"Because our logic is so lightweight—taking up just 1% of this low-cost FPGA—automakers can print this security circuit directly into the transceivers already used in every car for pennies. Aegis-CPS makes vehicles unhackable without changing a single wire."*
