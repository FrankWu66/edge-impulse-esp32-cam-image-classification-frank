import paho.mqtt.client as mqtt
import cv2
import datetime 
import time

MqttBroker="127.0.0.1"
MqttPort=1883
CloudTopic="frank/Clould_to_Edge"
EdgeTopic="frank/Edge_to_Cloud"
ISOTIMEFORMAT = '%H:%M:%S.%f'

index = 0
time_start = 0
time_end = 0

#Callback for connect.
def on_connect(client, userdata, flags, reason_code, properties):
    print("Edge 連線結果：" + str(reason_code) + "\n")
    #訂閱Cloud傳來的TOPIC
    client.subscribe(CloudTopic)

#callback for receive subscribe update.
def on_message(client, userdata, message):
    global time_start
    global time_end
    print ('received from Cloud: [%s]: %s' % (message.payload, datetime.datetime.now().strftime(ISOTIMEFORMAT)))
    time_end = time.time()
    time_interval = time_end - time_start
    print ('spend time (total): %.6f' % time_interval)
    print (" ")

mqttc = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
mqttc.on_connect = on_connect
mqttc.on_message = on_message
mqttc.connect(MqttBroker, MqttPort, 60)
mqttc.loop_start()
time.sleep(1)

'''
while True:
    payload = "===" + str(index) + "=="
    index+=1
    print ("send" + payload + "start:" + datetime.datetime.now().strftime(ISOTIMEFORMAT))
    mqttc.publish(EdgeTopic, payload)
    print ("finish send" + datetime.datetime.now().strftime(ISOTIMEFORMAT))
    #mqttc.loop()
    time.sleep(3)
'''

while True:
    index+=1
    if index > 6:
        index = 1
    img=cv2.imread(str(index) + '.jpg')
    byteArr = cv2.imencode('.jpg', img)[1].tobytes()
    print ("send " + str(index) + ".jpg start:" + datetime.datetime.now().strftime(ISOTIMEFORMAT))
    time_start = time.time()
    mqttc.publish(EdgeTopic, byteArr)
    #print ("finish send" + datetime.datetime.now().strftime(ISOTIMEFORMAT))
    time.sleep(3)