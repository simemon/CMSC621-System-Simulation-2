Assumptions -

1. Whenever new sensor connects to the Gateway, assuming it will be ON
2. Whenever new device conncets to the Gateway, assuming it will be OFF
3. First of all, Backend Gateway will start, Front-end Gateway will register with Backend Gateway.
4. Security device will start and complete its registration with Gateway and wait for message from Front-end Gateway.
5. Door will register from front-end gateway and wait for the message from front-end gateway till all the sensors connected.
6. Motion Sensor will register from front-end gateway and wait for the message from front-end gateway till remaining sensors connected.
7. Key Chain will register with front end gateway and then gateway will send information about all the sensors to all of them.
8. Then they will start sending messages to each other with vector clocks piggybacked to it.
9. Sensor Input Files for Motion and Keychain should start from 0 (as don't know which values to pass if it does not start from 0).
