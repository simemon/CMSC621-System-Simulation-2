gnome-terminal -e "bash -c \"./backend PersistentStorage.txt 1236; exec bash\""
sleep 4
gnome-terminal -e "bash -c \"./gateway ../SampleInput/Gateway_Config.txt GatewayOutput.log 127.0.0.1 1236; exec bash\""
sleep 4
gnome-terminal -e "bash -c \"./smart_device ../SampleInput/Device_Config_1.txt SecurityDeviceOutput.log; exec bash\""
sleep 4
gnome-terminal -e "bash -c \"./sensor1 ../SampleInput/Sensor_Config_2_1.txt DoorInput.txt DoorOutput.log; exec bash\""
sleep 4
gnome-terminal -e "bash -c \"./sensor1 ../SampleInput/Sensor_Config_2_2.txt MotionInput.txt MotionOutput.log; exec bash\""
sleep 4
gnome-terminal -e "bash -c \"./sensor1 ../SampleInput/Sensor_Config_2_3.txt KeychainInput.txt KeychainOutput.log; exec bash\""