Assumptions -

1. First of all, Backend Gateway will start, Front-end Gateway will register with Backend Gateway.
2. Security device will start and complete its registration with Gateway and wait for message from Front-end Gateway.
3. Door will register from front-end gateway and wait for the message from front-end gateway till all the sensors connected.
4. Motion Sensor will register from front-end gateway and wait for the message from front-end gateway till remaining sensors connected.
5. Key Chain will register with front end gateway and then gateway will send information about all the sensors to all of them.
6. Then they will start sending messages to each other with vector clocks piggybacked to it.
7. Sensor Input Files for Motion and Keychain should start from 0 (as don't know which values to pass if it does not start from 0).

8. INTRUDER AND USER ENTRANCE

a. Whenever Door close event fired, it start monitoring keychain and motion detector till door open comes
b. During Monitoring, if it finds both motion detection and key chain then user entered and turn off security system
c. During Monitoring, it if finds motion but not key chain then intruder entered
d. If both are not found, no one is in the house (user exited)

