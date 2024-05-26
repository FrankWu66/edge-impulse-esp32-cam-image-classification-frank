import paho.mqtt.client as mqtt
import cv2
import datetime 
import time
import os
import sys

MqttBrokerLocal="127.0.0.1"
MqttBrokerMqttgo="mqttgo.io"
MqttBrokerEclipse="mqtt.eclipseprojects.io"
MqttPort=1883
CloudTopic="frank/Clould_to_Edge"
EdgeTopic="frank/Edge_to_Cloud/#"
EdgeTopicPic="frank/Edge_to_Cloud/Pic"
EdgeTopicCR="frank/Edge_to_Cloud/ClassifyResult"
EdgeTopicPP="frank/Edge_to_Cloud/PicPerson"

face_cascade = cv2.CascadeClassifier("haarcascade_frontalface_default.xml")
ubody_cascade = cv2.CascadeClassifier("haarcascade_upperbody.xml")

ISOTIMEFORMAT = '%H:%M:%S.%f'

index = 0
personCount = 0
time_start = 0
time_end = 0
lastestTopic = EdgeTopicPic

path = datetime.datetime.now().strftime('%m_%d_%H_%M_%S_') + lastestTopic[20:]
if not os.path.isdir(path):
    os.mkdir(path)

'''
file_path = "randomfile.txt"
sys.stdout = open(file_path, "w")
print("This text will be added to the file")
'''

#Callback for connect.
def on_connect(client, userdata, flags, reason_code, properties):
    print("Cloud 連線結果：" + str(reason_code) + "\n")
    #訂閱Edge傳來的TOPIC
    client.subscribe(EdgeTopic)

def receive_and_save_pic (client, userdata, message):
    global personCount
    detectFace = False
    detectUbody = False

    #filename = 's_%d.jpg' % index
    filename = path + '\\s_%d.bmp' % index

    print('received from Edge: %s, size: %d' % (datetime.datetime.now().strftime(ISOTIMEFORMAT), len(message.payload)) )
    #print('received from Edge: [%s]: %s' % (message.payload, datetime.datetime.now().strftime(ISOTIMEFORMAT)))

    if len(message.payload) != 27702:
        # mean this payload not 96X96 BMP... so save it as jpg
        filename = path + '\\s_%d.jpg' % index
    
    f = open(filename,'wb+')
    f.write(message.payload)
    f.close()

    img = cv2.imread(filename)
    gray = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)  # transfer to gray img

    # detect face
    faces = face_cascade.detectMultiScale(gray)
    if len(faces):
        detectFace = True
    for (x, y, w, h) in faces:
        cv2.rectangle(img, (x, y), (x+w, y+h), (0, 255, 0), 1)   # mark face by green color #BGR
        cv2.putText(img, 'face', (x,y-5),cv2.FONT_HERSHEY_SIMPLEX, 0.3, (0,255,0), 1, cv2.LINE_AA)

    # detect upper body
    ubodys = ubody_cascade.detectMultiScale(gray)
    if len(ubodys):
        detectUbody = True
    for (x, y, w, h) in ubodys:
        cv2.rectangle(img, (x, y), (x+w, y+h), (255, 0, 0), 1)   # mark ubody by blue color
        cv2.putText(img, 'upper body', (x,y-5),cv2.FONT_HERSHEY_SIMPLEX, 0.3, (255,0,0), 1, cv2.LINE_AA)

    cv2.imwrite(filename, img)

    #img=cv2.resize(img, (640, 480))
    img=cv2.resize(img, (576, 576))
    cv2.imshow(message.topic, img)
    key=cv2.waitKey(1)  # wait 1ms for keyboard...
    #cv2.destroyWindow('image')
    
    if detectFace or detectUbody:
        personCount += 1

    #payload = 'C2E_' + str(index) + "_" + datetime.datetime.now().strftime(ISOTIMEFORMAT)
    payload = 'C2E_' + str(index) + ", detectFace: %d, detectUbody: %d" % (detectFace, detectUbody) #+ '\x00'
    return payload


#callback for receive subscribe update.
def on_message(client, userdata, message):
    global index
    global personCount
    global time_start
    global time_end
    global lastestTopic
    global path

    # reset index when change topic
    if message.topic != lastestTopic:
        index = 0
        personCount = 0
        lastestTopic = message.topic
        path = datetime.datetime.now().strftime('%m_%d_%H_%M_%S_') + lastestTopic[20:]
        if not os.path.isdir(path):
            os.mkdir(path)        

    index+=1
    time_start = time.time()

    if message.topic == EdgeTopicPic:
        #print ('EdgeTopicPic')
        payload = receive_and_save_pic (client, userdata, message)
    elif message.topic == EdgeTopicCR:
        #print ('EdgeTopicCR')
        payload = 'C2E_' + str(index) + ", got it."
    elif message.topic == EdgeTopicPP:
        #print ('EdgeTopicPP')
        payload = receive_and_save_pic (client, userdata, message)
        payload += ", accuracy: %.4f" % (personCount/index)

    client.publish(CloudTopic, payload)
    time_end = time.time()
    time_interval = time_end - time_start
    print ("finish send message:" + payload)
    print ('spend time (in cloud): %.6f' % time_interval)
    print (" ")


'''
#callback for receive subscribe update.
def on_message(client, userdata, message):
    time_start = time.time()
    print('received from Edge: [%s]: %s' % (message.payload, datetime.datetime.now().strftime(ISOTIMEFORMAT)))
    payload = str(message.payload) + datetime.datetime.now().strftime(ISOTIMEFORMAT)
    client.publish(CloudTopic, payload)
    time_end = time.time()
    time_interval = time_end - time_start    
    print ("finish send" + datetime.datetime.now().strftime(ISOTIMEFORMAT))
    print ('spend time (in cloud): %.6f' % time_interval)
    print ("  ")
'''
 
#setting MQTT connect   
mqttcLocal = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
mqttcLocal.on_connect = on_connect
mqttcLocal.on_message = on_message
mqttcLocal.connect(MqttBrokerLocal, MqttPort, 60)
mqttcLocal.loop_start()

mqttGo = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
mqttGo.on_connect = on_connect
mqttGo.on_message = on_message
mqttGo.connect(MqttBrokerMqttgo, MqttPort, 60)
mqttGo.loop_start()

mqttEc = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
mqttEc.on_connect = on_connect
mqttEc.on_message = on_message
mqttEc.connect(MqttBrokerEclipse, MqttPort, 60)
mqttEc.loop_forever()
