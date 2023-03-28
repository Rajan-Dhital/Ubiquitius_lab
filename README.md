# Ubiquitius_lab
Car parking routing system./
Two ground sesors to detect incomming and outgoing car from the parking space. Red singal when there is not a parking space available and green light for availability of the parking space.
Concept outline/
Assumptions: 
• The distance between two sensors is 50mm.
• By default, there are 10 parking space.
• Only one state machine is used.
• When the vehicle crosses both sensors then, count is increased if the vehicle is entering.
Communication:
Communication is done by parsing each character.
• Reset: when the state is at reset, count = 0, average = 0 and max = 10.
• Direction: dir variable is used to give direction. When dir is 0 then vehicle enters parking 
space when first S2 is pressed. When dir is 1 then to enter the parking space S1 is to be 
pressed first.
• Close/Open: Green led glows when the count is less than maximum no of parking space 
and red glows if count is equal to maximum no of parking space.
• MAX: In this state user can modify the parking space for certain area.
• Monitoring: when the state machine goes to respective state then the number of vehicles
and average speed is displayed on the terminal.
Opening and closing:
• Real time clock(B) is used in alarm enable mode. Hour alarm is enabled at 8’O clock.
• When the time is 20:00’ O clock then the system is set to LPM3.5 sleep mode. Both buttons 
are disabled before this mode and the important information (count, direction, storage) are 
stored in FRAM, which can be accessed when the system wakes up from the sleep mode.
• When the system is waked up then the hour alarm is changed to 20:00’ O clock again. At 
the waked state if none of the interrupt is working then the system goes to LMP4 sleep 
mode.
Counting and speed measurement:
If the direction is 0 and first S2 button and then S1 button are pressed twice, then count value is 
increased. Count is increased when the vehicle crosses both sensors. If one vehicle enters the 
parking space, if at this point the parking space becomes full then red led will blink for 200ms. 
Similarly, if the car leaves the parking space when the space was full, then green led blinks for 
200ms.
Once any button is pressed between two, then the capture mode of timer is activated. When the 
next button is pressed then at the same time, time interval from first button to next is captured, this 
time interval is used to compute speed of the specific vehicle. Basically, time interval between the 
triggering of sensors by front wheel is captured
