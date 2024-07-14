import paho.mqtt.client as mqtt
import cv2
import datetime 
import time

MqttBroker="127.0.0.1"
#MqttBroker="mqttgo.io"
#MqttBroker="mqtt.eclipseprojects.io"
MqttPort=1883
CloudTopic="frank/Cloud_to_Edge"
EdgeTopicPic="frank/Edge_to_Cloud/Pic"
EdgeTopicCR="frank/Edge_to_Cloud/ClassifyResult"
EdgeTopicPP="frank/Edge_to_Cloud/PicPerson"
ISOTIMEFORMAT = '%H:%M:%S.%f'

index = 0
time_start = []
time_end = 0
cycle = 0
total_time = 0
publish_count = 0

#Callback for connect.
def on_connect(client, userdata, flags, reason_code, properties):
    print("Edge 連線結果：" + str(reason_code) + "\n")
    #訂閱Cloud傳來的TOPIC
    client.subscribe(CloudTopic)

#callback for receive subscribe update.
def on_message(client, userdata, message):
    global time_start
    global time_end
    global cycle
    global total_time
    cycle += 1
    print ('received from Cloud: [%s]: %s' % (message.payload, datetime.datetime.now().strftime(ISOTIMEFORMAT)))
    time_end = time.time()
    time_interval = time_end - time_start[cycle-1]
    total_time += time_interval
    print ('spend time (total): %.6f    count:%02d, avg time: %.6f' % (time_interval, cycle, total_time/cycle))
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
    time_start = time.time()
    mqttc.publish(EdgeTopic, payload)
    #print ("finish send" + datetime.datetime.now().strftime(ISOTIMEFORMAT))
    #mqttc.loop()
    time.sleep(1)
'''

while publish_count < 6:
    #while True:
    index+=1
    publish_count += 1
    if index > 9:
        index = 1
    img=cv2.imread(str(index) + '.jpg')
    #imgBMP=cv2.imread(str(index) + '.bmp')
    byteArr = cv2.imencode('.jpg', img)[1].tobytes()
    #byteArrBMP = cv2.imencode('.bmp', imgBMP)[1].tobytes()
    print ("send " + str(index) + ".jpg start:" + datetime.datetime.now().strftime(ISOTIMEFORMAT))
    #time_start= time.time()
    time_start.append(time.time())
    mqttc.publish(EdgeTopicPic, byteArr)
    #time.sleep(3)
    #time_start.append(time.time())
    #mqttc.publish(EdgeTopicPic, byteArrBMP)
    '''
    if index > 5:
        mqttc.publish(EdgeTopicPic, byteArr)  # EdgeTopicPic EdgeTopicCR EdgeTopicPP
    else:
        mqttc.publish(EdgeTopicPP, byteArr)
    '''
    #print ("finish send" + datetime.datetime.now().strftime(ISOTIMEFORMAT))
    time.sleep(3)

'''
while cycle < 300:
    time.sleep(1)

time.sleep(3)
'''