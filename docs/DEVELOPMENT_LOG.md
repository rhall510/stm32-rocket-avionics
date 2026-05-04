# **Progress log**



Regular detailed progress logs will be written here. Most recent at the top.

---
### May 3rd 2026

Over the past two days I have finished implementing and debugging data logging and a basic download link between the main avionics board and a host PC. Data logging has been more thoroughly tested now seems to work perfectly after ironing out a few issues. See the test logs [here](dev_board_planning/Data%20logging%20tests.txt) for more details.


I also spent a lot of time testing and debugging the data transmission function. I made two 2.4GHz ground plane monopole antennas in the same design as the previous 868MHz ones and connected them to the transmitter (avionics unit) and the receiver (nucleo-g431kb board connected to the host PC). Unfortunately, as I only have the nanoVNA-H which only measures up to 1.5GHz, I cannot tune the antennas perfectly for 2.4GHz so I just cut them to as close to the theoretical perfect length as possible.

#### 2.4GHz ground plane monopole antenna

![2.4GHz ground plane monopole antenna](./images/2.4GHz_monopole.jpeg)

#### View of the fully assembled avionics unit with the 2.4GHz antenna and ST-Link attached

![Avionics unit with 2.4GHz antenna](./images/avionics_dev_v2_fullview_live.png)

Initially the receiver was not receiving packets but it seems most of the issues were caused by some incorrect configuration options and not properly waiting for the transmitter to finish sending a packet before queueing the next one. After fixing the options, implementing proper delays, and adding a proper header and terminator to each packet we now have a working data download link which can transmit data to a host PC via a connected receiver over UART at around 200kb/s. There is currently no error correction in the download protocol though.

The next step is to properly decode and parse the raw binary data received and implement error correction in case of any missing or malformed packets.

---
### May 1st 2026

Today I finished the main functionality of both LAMBDA drivers and got most of the way through writing functions to enable data logging and transmission over 2.4GHz radio. There are two main bits of functionality added, the first being centered around the WriteFullDataPacket. This function writes all accumulated data from sensors to the flash memory periodically (currently at 10Hz controlled by TIM2). Data is accumulated in two identical buffers which operate in a 'ping-pong' fashion. Buffer A is filled initially with all data until the periodic write is triggered, at which point everything accumulated in buffer A is written to flash while all new data is accumulated in buffer B. On the next write the opposite happens, with buffer B being written to memory while data accumulation switches back to buffer A. While all functions are currently blocking for testing simplicity, in the future data collection will be among the highest priority tasks and so may take place in the middle of a write. This method prevents new data being written into the buffer currently being committed to flash which could cause data loss. Data accumulation into the buffers is handled by 'AppendLogPacket' functions specific to each sensor, which handles packaging their data into byte arrays and appending them to the buffer.

The second main bit of functionality is the data transmission. There is no USB or UART connection on this board currently so I plan to use the LAMBDA80 module to transmit the stored data to a host PC over 2.4GHz radio. While this will take longer than a wired connection, using the highest data rate modulation settings makes the expected transfer times manageable, though wireless transmission is more error prone and so will require error correction mechanisms. I have currently implemented a very simple function which reads all flight data and transmits it half a page at a time in blocking mode. To facilitate this, I added counters for the number of bytes and packets contained in the data section of flash which are initialised and maintained by the driver. There is currently no packet checking for error correction and only the transmit side is implemented.

I also finally got the desoldering iron I ordered and was able to switch the LAMBDA modules into their correct places using it, meaning I can now begin testing the LAMBDA drivers. First though, I need to make some 2.4GHz antennas (likely a ground plane monopole design like the 868MHz ones) and make a receiver device that can pass the data to the host PC. I plan to do this using the stm32 nucleo-g431kb boards I got a while ago and some simple breadboard circuitry so that I can get it up and running ASAP.

---
### Apr 29th 2026

Over the past few days I've been working on integrating the two LoRa transceivers. I soldered the LAMBDA62 and LAMBDA80 modules to the board only to realise I accidentally placed them the wrong way around. Although they have the same general pin layout, the LAMBDA62 has an additional two lines for the RX and TX switch pins so to be fully operational without bodge wires I need to swap them back. I tried desoldering them with solder wick but I cannot get enough of the solder out the through holes to lift them off safely. I have ordered a desoldering pump to help with this.

In the mean time I've made a start on the software side writing basic drivers to test both modules, starting with the LAMBDA62. At the moment I have written functions to do all needed functions like transmitting packets, receiving packets, and emitting a constant wave. These are all written as simple blocking functions currently for ease of testing but later they will be modified to be real time compatable by using DMA for the large buffer SPI transfers and EXTI interrupts on the BUSY pin to replace the current blocking polling loops. EXTI interrupts will also be used for the DIO pins to notify the MCU on packet transmission/reception. Unfortunately, DIO1 for the SX1262 (LAMBDA62) is unavailable as an EXTI interrupt as the LSM6DSR uses pin 4 as it's data ready interrupt. I didn't realise at the time of designing the PCB that EXTI interrupts cannot share pin numbers despite being in different ports. However, for the intended purpouse of simply sending a constant wave, the SX1262 does not need this second interrupt line.

I am currently in the process of writing the LAMBDA80 driver, which will use mostly the same code as the LAMBDA62 driver, just adapted to the specifics of the SX1280 instead. Once I can desolder and swap the modules I will be able to test the drivers proerly, although I will need to make 2.4GHz antennas for the LAMBDA80 first.


---
### Apr 25th 2026

Today I added some more functions to test the W25Q NOR flash driver. Here is a summary of the tests.

As detailed in yesterdays log, the driver can correctly boot up, read the metadata and flight data sections, erase the old data, and write new data. It also correctly handles wear levelling by incrementing the metadata and flight data section addresses on each write cycle.

Next I tested the bad block scanning function. It was able to complete the scan successfully and found no bad blocks as expected from a lightly used NOR flash chip.

Since there are no actual bad blocks, I expanded the bad block test to inject a fake bad block flag for the block starting at address 0x20000 (the 3rd block) and test writing and reading back a ~200KB chunk of data. The test wipes the first 10 blocks of flash before starting (to avoid a lengthy full chip wipe) which removes the existing metadata and data sections and causes blank ones to be created. This yields a repeatable setup where the metadata sector spans addresses 0x10000 to 0x11000, the bad block spans 0x20000 to 0x30000, and the data section starts at 0x1000 (starting empty). The test then writes approximately 200KB of data which, if correctly written, should expand the data section to place it's end address at 0x3FF8B. When the test was executed this exact behaviour was observed, confirming the writes were handled correctly and skipped the metadata section and bad block. Next the data was read back, starting from address 0x1000 and reading a length of 200KB. The data readout showed the data was written perfectly, with byte order being preserved across skipped sections and across pages. An additional initialisation test performed directly after this test where the chip is booted and reads the metadata/data sections confirmed it was able to read this written data perfectly from a fresh boot as well. See the test logs [here](dev_board_planning/W25Q%20bad%20block%20test%20log.txt).

Finally, I tested the wraparound behaviour at the end of memory. For this I wrote a function which initialised an empty metadata and data section by wiping existing data and then moved the empty data section start and end addresses to the last sector address in memory (0x1FFF000). It then writes 25 pages of data and reads it back from the starting address. This first test confirmed both the write and read functions correctly handle wraparound behaviour at the end of memory, perfectly preserving bytes across the transition. I then modified the test to move the data section back 1 block and inject a fake bad block in the last block, to test whether it could handle skipping a section and wrapping around at the same time. This second test worked perfectly as well. The third and final test just involved moving the data section to the original last sector address, which is now inside the fake bad block. Again, this was handled perfectly, with data being immediately wrapped around and written to the start of memory. See the test logs [here](dev_board_planning/W25Q%20wraparound%20test%20log.txt).

These tests confirm my W25Q driver is working as expected and I can now move on to the next steps, which I think will be adding in the final pieces of the board, the LoRa transceivers, to enable a command link and transfer of data.

---
### Apr 24th 2026

Finished implementing the rest of the functions needed for the NOR flash driver. These include:
- Read volume functions to read blocks of data in pages. The safe version also respects bad blocks and metadata sections and wraps the address if it extends beyond the end of memory.
- Split the safe address functions into 3 more specialised and robust functions which handle write addresses, full read addresses, and offset read addresses (for skipping around the data section only reading headers).
- Erase functions to erase either the full chip or specific blocks or sectors.
- Function which erases previously written flight data and automatically increments the data section start address and updates the metadata section.
- Function which detects and records bad blocks by writing a specific pattern to each page of memory and checking for readback errors.

I also wrote a small test function to check the W25Q is working correctly. It checks initialisation, erasing data, writing a test packet, and appending to already written data with another test packet. The first test using this function seemed to work perfectly, with all tests passing, however, when relaunching the debug session to check for correct incrementing of metadata and data addresses I encountered unexpected behaviours. At first every test after the first completely failed as the metadata section could not be found. Stepping through debug mode revealed the rx buffer was reading all zeroes on each block start address. The cause of this was eventually identified to be likely because of the erase sector and erase block functions not working correctly. Only the command was being sent, omitting the address to erase which would cause the command to be dropped or potentially cause other strange behaviour. After fixing this the data was able to be read normally.

The tests passed on each restart of the debug session now but the address incrementing was abnormal. Based on the data logging architecture we would expect the metadata address to increment by 1 block length each cycle and the data address to increment by 1 sector each cycle, except for the first cycle after a full erasure (as the empty data section would cancel the call to EraseFlightData and prevent address incrementing). In this case though, the first test after a full erase was normal, the second test showed the metadata and data start addresses did not change from the first test, and the third and fourth tests showed each address incremented twice the expected amount. Eventually I realised this was due to the slight delay in restarting a debug session allowing the MCU to boot and start executing code independently of the debug session. This caused the erase code to run at the start of the second test, meaning the addresses did not increment as expected. Then in the third and fourth tests the initialisation cycle was completed twice, once befor the debug sessions started and once after, causing the double increment. This has been temporarily dealt with by adding a 2s delay at the start of the main loop to prevent code from executing in the window between debug sessions. After adding the delay the addresses increment as normal and all tests pass perfectly.

The next priority is to test the bad block management and writing longer volumes which span multiple pages and stretch across block and memory boundaries.

---
### Apr 22nd 2026

Started developing driver firmware for data logging on the NOR flash chip (W25Q256JVEIQ). The data logging architecture plan can be seen [here](dev_board_planning/Data%20logging%20architecture.txt) which implements wear levelling and bad block management while clearing the way for fast data logging during flight where performance is critical. So far I have been working mainly on the startup sequence, gathering metadata and preparing functions for writing data safely.

Functions implemented:
- Initialisation function which resets the device, checks it is responding correctly, and sets it to 4 byte addressing mode for future commands.
- Get safe address functions. Perform checks to ensure all data is only written to a valid address (i.e. within memory bounds, not in bad blocks, (for non-metadata) not in the metadata or data sections).
- Read metadata function. Automatically scans the entire address space to find any existing metadata sectors. If found then it parses the information contained within to find the start address of the data section as well as any saved bad blocks. If not found then it automatically creates a blank metadata section at the beginning of memory.
- Write metadata function. Finds the first valid metadata sector address and writes current metadata to that sector. Optionally erases the previous metadata sector first.
- Write volume and page program functions. Work together to enable safe writing of large contiguous blocks of data only to safe addresses. Performs checks and automatically skips bad blocks and already written sections. Also splits the input data into chunks to fit within each page boundary safely.


To be implemented next:
- Functions to read volumes of data
- Functions to erase sections or the entire chip
- Function to scan for bad blocks
- Reconfigure initialisation sequence to properly use the above functions according to the [data logging plan](dev_board_planning/Data%20logging%20architecture.txt) (currently it just issues some basic commands)
- Implement planned read and write modes

<br>

---
### Apr 19th 2026

The 3rd board utilising the new V2 design was brought up successfully! Every important line was tested for shorts with a multimeter prior to closing the power isolation jumpers to each section one by one, all of which powered on successfully. The only section not brought up was the magnetometer/barometer section which contained a short between power and ground due to bridging under the MMC5983MA. Due to the lack of a working hot air gun firmware development was started with this section disconnected for a couple of weeks.


#### Firmware development pt.1
Initial tests using STM32CubeIDE with basic startup code showed the MCU was working perfectly and could interface with the peripheral devices like the user LED and accelerometers. The HAL library is used for functions like I2C and SPI communication to handle them robustly and get the board up and running quickly. Currently all communications operate in blocking mode for testing but this will be changed to use non-blocking DMA when moving to an RTOS architecture.

I started by writing drivers to handle the LSM6DSR and ADXL375 accelerometers, both on SPI bus 1. Both are set to sample measurements at 100Hz and send interrupts when their FIFO buffers contain a configurable number of measurements (currently 4). These trigger EXTI interrupts which set a 'data ready' flag and cause the MCU to initiate reading from the FIFO in the next loop. Data from the FIFO is read and the raw bits are converted into readable measurements and stored in buffers which are overwritten each time a new set of measurements comes in.

Before I could develop drivers for the other sensors (GPS, barometer, magnetometer) I had to fix the solder bridging under the MMC as it not only created a dangerous short between power and ground, meaning I couldn't power the magnetometer and barometer, but it also shorted the I2C bus SDA and SCL lines, meaning the GPS on the same bus could not be used.


#### Repairing the MMC
Eventually I got my hot air gun working again and attempted to repair the MMC. I tried applying some flux paste and reflowing with the hot air gun without lifting the chip off first but after a while of heating this did not resolve the bridges. I then tried removing the chip, wicking the pads on the PCB and chip flat, applying a small layer of solder on the PCB pads, and then reflowing the solder with flux while the chip sat on top. This did resolve the bridges and solder the chip properly but the MMC was unresponsive to an I2C scanner or direct I2C communication attempts. I now realise this is likely due to the extreme heat applied to it by prolongued hot air gun heating and wicking the pads with a soldering iron. A second attempt to transplant the MMC from the first PCB attempt (to which power was never applied) also ended identically. The chip soldered well but failed to respond likely again due to extended hot air exposure at temperatures too high. I attempted to solder a third fresh MMC to the board, this time trying to be more cautious with hot air exposure, but it also ended the same way.

It was at this point I realised that too much heat exposure must be the issue. The PCB schematic looked good after triple checking it and the soldering seemed to be working ok, not causing any bridges and soldering the pads properly (checked by testing the input diodes on the pins). I performed a series of thermal tests to identify the most conservative possible hot air conditions I could use to solder the chip from the underside of the PCB to avoid direct hot air exposure (see [thermal tests](../hardware/avionics/avionics_dev_board/Hot%20air%20tests.md)), and switched to using leaded solder to reduce the melting temperature needed. After soldering another fresh MMC using the newly identified more conservative conditions and verifying no shorts were present the board was powered on and the MMC responded to I2C calls! Subsequent software testing revealed that the SET/RESET function of the MMC does not seem to be working so more testing needs to be done to figure out why. The CAP pad may not have soldered correctly for example. This is non essential for the function of the MMC though as a different and more robust reading calibration method will eventually be implemented.

#### Firmware development pt.2
After successful repair of the MMC I was able to develop drivers for all remaining sensors, all of which use the same I2C bus as they are all slower sensors operating at 10Hz. Apart from the MMC SET/RESET issues mentioned above driver development for all these sensors went very smoothly and they all seem to work just fine. I have not been able to fully test the GPS module yet however as testing the board indoors prevents it from acquiring a satellite lock. It does output valid empty NMEA sentences however, so it seems to be working. To fully test it I will need to implement data logging and take it outside.


#### Data logging
Currently I am working on writing a driver for the NOR flash chip to enable data logging and readback. I have implemented a startup function and am currently trying to figure out a good structure to store the data efficiently and robustly. Below is my current plan for storing the data:

Buffers fill up with raw data that has been collected since the last write. Data is logged at 10Hz, at which point whatever is in the buffers is concatenated into a 'packet' that is sent to be written to storage. Each packet has a packet header with a sync word and information about the contents of the packet, followed by each piece of data which have their own headers containing information on the size and type of data they represent.

**Packet structure:**
\>> 4 Byte start sync word (AABBBBAA)
\>> 4 byte header: Total number of readings in packet [31:24], Total number of bytes in packet [23:8], Checksum [7:0]
[[[
\>> 2 byte header: Data type [15:12], Total number of bytes in data [11:0]
\>> Raw data (variable length)
]]] x N



### Note on updates going forward
From now on I am going to try and write these updates more informally and more fequently, almost for every day I work on something ideally. This will mean the updates will be smaller but overall contain much more detail as I will be able to accurately describe everything I've done and the decisions made, which become more difficult to remember after a while. It also reduces the friction to writing these updates which has been a problem. I hope that writing more frequent updates will also serve as a better look into the process, rather than just the results, of this project which I (and hopefully others) can look back on later.





---

### Apr 1st 2026

Further testing of the second avionics dev board ultimately seemed to suggest the board had some sort of catastrophic failure which has now left it unrepairable. Thermal imaging while powering the board bypassing the buck converter using a current limited benchtop power supply set at 500mA showed uniform heating across multiple components (the MCU, GPS module, accelerometers etc.). The amount of heating rose proportionally with the current limit set on the power supply.

#### Thermal imaging of the avionics dev board over time

![Thermal imaging of the avionics dev board over time](./images/avionics_dev_att2_thermal.jpg)

Additionally, attempts to find any dead shorts by measuring the voltage drop between GND and various points around the board gave uniform readings for every point tested. Due to the lack of power isolation mechanisms on the board it was not possible to disconnect sections of the board to test individually, however, after desoldering the inductor in the voltage regulator section to isolate the buck converter from the rest of the board I was able to measure dead shorts on both sides. These tests point towards the chips on the board having been damaged by an overvoltage event during the initial power up, potentially caused by a poorly soldered or dysfunctional buck converter outputting too much voltage.

<br>

### Avionics dev board V2

To prevent the next board facing the same fate I decided to redesign some parts of the board to improve the bring up phase and testability, as well as adding some extral QoL things. The second version of the avionics dev board includes the following changes from V1:

- Changed SWD interface pins to STDC14 form factor
- Added a user LED and button to the free MCU pins
- Added solid ground and power test points in the middle of the board
- Add jumpers to isolate power to the main power rail and to individual sections of the board
- Switched to using smaller buttons to save space
- Increased distance between headers for the LAMBDA modules as they use non-standard spacing
- Switched to a lower 500mA but slower blowing fuse to prevent potential issues with inrush current
- Rearranged the fuse to be the first component after the power switch
- Decreased resistor values on LEDs from 1kR to 220R

#### Full board view (red = top signal layer, blue = bottom signal layer)

![Full view of avionics dev board v2](./images/avionics_dev_v2_fullview.png)

The new PCB design and fresh components were ordered and I have begun assembling the board. I am only assembling components as and when they are required this time to prevent any unneccesary damage if something goes wrong again. So far, all SMD components were soldered via hotplate reflow as before, and the main power terminal, power isolation jumpers, and switches were soldered manually. Extra care was taken to not apply too much solder paste as this may have caused the issues seen in the second attempt of V1. A few minor bridges between MCU and buck converter pins were resolved after reflow though there is still some potential bridging under the MMC magnetometer module which is difficult to assess with my current tools. I have ordered some smaller multimeter test probes to enable easier measurements of lines connecting to the MMC module to help diagnose any potential issues before bringing up the board completely.

Before the potential MMC bridging is resolved though I tested the power regulator section of the board by disconnecting all power isolation jumpers, ensuring the regulator did not connect at all to the rest of the board. First I probed all the pins of the buck converter to ensure there were no shorts and they were connected properly. I also tested a few other parts for shorts and checked diode orientation and fuse continuity. All of these tests passed. Then for an initial test to ensure the buck converter worked safely I used a benchtop power supply to supply 9V at a low current limit of 35mA. When this power was supplied, the buck converter output a stable 3.3V and thermal imaging confirmed no components were heating up substantially.

I next plan to use the multimeter to thoroughly test and resolve any issues with the rest of the board before bringing up each section individually.

---

### Mar 10th 2026

The PCB and components arrived and I was able to assemble the PCB using hotplate reflow. The first attempt went well except for a misaligned MCU which meant each pin in one axis was offset by 1. I tried to correct this by using a hot air station to move the MCU but it seemed to be not heating it well enough to melt the solder. I now know I should have used the hot air station while the board was soaking in 150C heat on the hotplate. Instead I tried using a soldering iron to melt the solder and move the MCU but this resulted in many of the pins being bent beyond repair.

Luckily I had bought two sets of every component for exactly this scenario so I attempted it again the next day. This time making sure the MCU and all other components were more precisely aligned. After reflow all components appeared to be correctly aligned and a few minor bridges between MCU and sensor pins were corrected using the soldering iron. Through hole components were added on to complete the board.

After assembly the board was tested with a fresh 9V battery. Upon flipping the power switch on I saw what looked like a brief flash of light in the voltage regulator area and no light from the power LED. Testing with a multimeter revealed the 1A fuse just before the buck regulator had blown and further testing revealed a short circuit is present between ground and power somewhere on the board. In order to locate the short circuit I have ordered a benchtop power supply and thermal camera to pump a limited current into the circuit and observe hotspots.

#### Assembled dev board in it's current state

![Full view of avionics dev board v1 assembled](./images/avionics_dev_v1_fullview_live.jpg)

---

### Feb 28th 2026

While waiting for components to arrive I've been learning about the general STM32 MCU startup process and writing some very simple bare metal startup code for the STM32G431KB MCU. This is able to be flashed onto the STM32G431KB-NUCLEO board and the loop counter can be seen increasing using GDB and OpenOCD.


---

### Feb 12th 2026

First design interation of the avionics development PCB has been completed. The PCB and components have been ordered and I will begin assembly and testing when they arrive.

#### Full board view (red = top signal layer, blue = bottom signal layer)

![Full view of avionics dev board v1](./images/avionics_dev_v1_fullview.png)


The board is organised roughly into 5 sections:
- Top right: Power supply and voltage regulator. Power is supplied by a 9V battery across the J1 terminal and stepped down to 3V3 by a TPS563201 buck converter.
- Top middle/left: LoRa transceivers and antenna SMA connections.
- Middle right: GPS module and antenna.
- Bottom right: Parachute deployment system. Utilises a separate power supply driven by a MOSFET to burn a length of nichrome wire.
- Bottom left: Main MCU and sensor block.

This board uses a 4 layer stackup of SIG-GND-PWR-SIG, enabling easy distribution of power throughout the board. To minimise return path issues, only not critical signals are routed on the bottom signal layer with the power layer as it's reference. The only signal this was not possible for is the GPS antenna line, for which a separate island was made on the power layer surrounding the trace tightly coupled to ground using many vias. Along with this, 4 vias were placed to surround the signal via when it transitions back up to the top layer to ensure it always maintains a close ground reference.

#### GPS antenna trace shielding

![GPS antenna trace shielding](./images/gps_antenna_trace_shielding.png)


0602 footprints were used for most passives except large value capacitors. I plan to assemble the board using a stencil to apply solder paste and a hot plate to reflow. Through hole components can then be soldered manually afterwards.

---

### Feb 1st 2026

The issue causing miscounting of packet sequence numbers by the receiver in the LAMBDA62 rangetest setup has been identified and fixed. Testing confirmed this was caused by the transmitter repeatedly resetting and starting the transmission counter from 0 again. The 9V battery powering the transmitter was outputting a much lower voltage than expected (around 7V, while the receiver battery output almost 9V) so a fresh battery was used. This fixed the issue and transmission was able to continue uninterrupted while the receiver correctly counted each packet. To mitigate the chance of this issue happening in the field spare batteries will be brought.

Component selections for the avionics unit have been finalised. Changes from the previous selections are detailed below:

- MCU = STM32G474RET6; The chip will be used rather than a nucleo board for compactness and to reduce cost.
- Storage = W25Q256JVEIQ; NOR flash will be used instead of NAND flash to increase reliability for storing critical flight data. This still offers more than enough storage capacity to write flight data for multiple long launches if needed.
- IMU = LSM6DSR; Switched to the R variant for lower risk of gyro saturation for negligible increase in cost.
- Pressure/temp = BMP581; Mainly due to lack of availability of the BMP390, but also slightly increased sensitivity.
- Magnetometer = MMC5983MA; Highly improved performance with degaussing option to remove offsets caused by other nearby components.

---


### Jan 28th 2026

Started designing the development PCB for the avionics unit, beginning with the power source. This utilises a TPS563201 buck converter to provide a stable 3.3V DC source from a 4.5V-17V input. Component values were taken from the datasheet specifications with resistor R2 being 33kΩ to get a 3.3V output. A space for capacitor C4 was added despite likely being unneeded so that the pads are there for easy addition later if testing shows it is needed. A switch was also added between the positive terminal input and the TPS563201 enable pin to allow the power source to be easily switched on and off. Temporary footprints have been used for the moment until actual component models are decided.

#### Schematic view

![Avionics dev board schematic view](./images/avionics_dev_buckconv.png)

#### PCB view

![Avionics dev board PCB view](./images/avionics_dev_buckconv2.png)

#### 3D view

![Avionics dev board 3D view](./images/avionics_dev_buckconv3.png)

---

### Dec 2025 - Jan 2026

This is a summary of the progress made in the first couple months of the project before I started taking logging more seriously.

I started the project by making prototype breadboard circuits to test communication between two LAMBDA62 modules, which contain the SX1262 868MHz transceivers used for this project. Arduino nanos were used to drive the transceivers through TXS0108E bidirectional logic level shifters to match the 5V Arduino logic to the 3.3V logic of the LAMBDA62 module. See the below [wiring schematic](../hardware/prototyping/LAMBDA62_rangetest/RangeTest.kicad_sch) and photo of the assembled rig.

#### Wiring schematic

![LAMBDA62 range test wiring schematic](./images/LAMBDA62_range_test_wiring_schematic.png)

#### Assembled test rig

![LAMBDA62 range test setup](./images/LAMBDA62_range_test_setup.jpg)


The antenna was an 868MHz ground plane monopole made from 1mm diameter bare copper wire soldered onto a SMA female panel mount connector. The monopole was originally cut to be 90mm long while the radials were cut to extend 100mm from the edge of the panel mount. A nanoVNA was used to tune the antenna by cutting the monopole until the point of minimum SWR was close to 868MHz. The final antenna and nanoVNA test results are shown below.

#### Final antenna

![868MHz ground plane monopole](./images/868MHz_monopole.jpg)

#### NanoVNA test results

![868MHz monopole nanoVNA test results](./images/868MHz_monopole_nanoVNA.png)

<br>

A simple lightweight Arduino sketch ([LAMBDA62_rangetest.ino](../firmware/prototyping/LAMBDA62_rangetest/LAMBDA62_rangetest.ino)) was made to drive the LAMBDA62 using only the SPI library. This sketch can be uploaded to the transmitter and receiver by changing the 'receiver' variable at the top. The transmitter will transmit the payload at the soonest avaiable opportunity with a 20ms delay in between packets and the receiver will receive each packet and determine whether it was sent correctly, along with measuring the strength of the signal. This information is output through the serial port of the Arduino to a connected computer running the [SerialMonitor.py](../firmware/prototyping/LAMBDA62_rangetest/SerialMonitor.py) script, which parses the information and outputs summary statistics every 50 packets to the command line and a text file while writing individual packet statistics to a .csv file.

A range test was conducted on a long flat beach using this setup. The transmitter was stationary on a plastic cutting board on the ground while the receiver was moved away as it received packets. The results can be found [here](./initial_range_tests/01-01-26_results/) graphs of the per packet RSSI/SNR over time and SNR vs RSSI are below. The results were much worse than expected as packets began being missed after only 200m of distance, and all reception was lost after ~600m. However, it is very likely this was caused by bad testing conditions. Placing the transmitter on the ground likely greatly reduced signal transmission efficiency due to ground proximity effects. The weather was very windy which meant the transmitter had to be touched for a significant portion of the test, likely also reducing the antennas radiation effectiveness due to body coupling. In addition, the receiver was moved by holding the breadboard it was attached to and letting it hang underneath held by the coax cable. Walking motions and the wind therefore cause significant polarisation mismatch as the receiver swung around. Another issue encountered during the test was the miscounting of packet sequence numbers by the receiver, causing the missed packet calculations to be significantly off. This issue was not observed during testing before the range test. The issue may have been caused by the receiver not correctly detecting packets with CRC errors, resulting in wrong data being decoded for the sequence number which then throws the calculations off. This was not tested before the first range test as I had no reliable way to induce CRC errors.

#### Per packet RSSI and SNR over time

![Per packet RSSI and SNR over time](./images/rangetest1_strengthVStime.png)


#### Per packet SNR vs RSSI

![Per packet SNR vs RSSI](./images/rangetest1_snrVSrssi.png)

Although noisy and less than ideal, the SNR vs RSSI relationship shows promise for the feasibility of using the AD8302 module with 20-30dB gain LNAs for PDOA tracking. The SNR does not begin appreciably dropping until an RSSI of around -90dBm, which shows the 868MHz signal is still received significantly above the noise floor in the RSSI range of the AD8302 (-60dBm) + 20-30dB LNA (-90dBm). Therefore it should be able to reliably process the CW tone uncomplicated by noise until its strength falls out of range.

To ensure the next range test provides more useful results the following modifications will be made:
- Fix the packet sequence number bug which is potentially caused by incorrect handling of erroneous packets. Test this by inducing corrupted packets, potentially by using an implicit/explicit header mismatch between the transmitter and receiver.
- Make a testing enclosure to protect and hold the components in place more rigidly. This will help protect the electronics from outside conditions and hold the antenna in place on an insulating material to ensure a better polarisation match. Additionally, they can be held high off the ground using handle underneath to reduce ground and body coupling effects.

The testing enclosure has been modeled and 3D printed with PETG (see model files [here](../mechanical/simple_range_test/) and images below).

#### Range test enclosure (closed - handle on the left screws into the base)

![Range test enclosure closed](./images/Range_test_enclosure1.jpg)

#### Range test enclosure (open)

![Range test enclosure open](./images/Range_test_enclosure2.jpg)

