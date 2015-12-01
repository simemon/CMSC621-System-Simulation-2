mkdir -p output/
cd data/
gcc backend.c -o ../output/backend -lpthread
gcc gateway.c -o ../output/gateway -lpthread
gcc smart_device.c -o ../output/smart_device -lpthread
gcc sensor1.c -o ../output/sensor1 -lpthread