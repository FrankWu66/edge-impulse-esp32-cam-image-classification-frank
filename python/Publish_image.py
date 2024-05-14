import paho.mqtt.client as mqtt
import random
import json  
import datetime 
import time
import cv2

# 設置日期時間的格式
ISOTIMEFORMAT = '%m/%d %H:%M:%S'
PubTopic1="frank/transfer_image/pic"

# 連線設定
# 初始化地端程式
client = mqtt.Client()

# 設定登入帳號密碼
#client.username_pw_set("try","xxxx")

# 設定連線資訊(IP, Port, 連線時間)
client.connect("127.0.0.1", 1883, 60)

while True:
    #t0 = random.randint(0,30)
    #t = datetime.datetime.now().strftime(ISOTIMEFORMAT)
    #payload = {'Temperature' : t0 , 'Time' : t}
    #print (json.dumps(payload))
    img=cv2.imread('dog1.jpg')
    print ("read dog1.jpg ok");
    #要發布的主題和內容
    byteArr = cv2.imencode('.jpg', img)[1].tostring()
    client.publish(PubTopic1, byteArr)
    print ("publish ok");
    time.sleep(500)