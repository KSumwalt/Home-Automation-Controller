# Home-Automation-Controller
Main controller for the Home Automation system.  Button Groups are used to send CAN Bus messages to the controller to control workstations. The workstations are relay(s) and/or MQTT messages.  A CAN Bus message is then sent out to any of the Button Groups which control this workstation to turn on/ff an LED back-light  to the buttons which control this workstation.
