#請先安裝1.paho-mqtt、2.opencv
import paho.mqtt.client as mqtt
import cv2
MqttBroker="127.0.0.1"
MqttPort=1883
SubTopic1="frank/transfer_image/pic"
 
#設定連線成功時的Callback
def on_connect(client, userdata, flags, rc):
    print("連線結果：" + str(rc))
    #訂閱主題
    client.subscribe(SubTopic1)
    
#設定訂閱更新時的Callback
def on_message(client, userdata, msg):
    f = open('receive.jpg','wb+') #開啟檔案
    f.write(msg.payload)#寫入檔案
    f.close()#關閉檔案
    #顯示影像檔
    img=cv2.imread('receive.jpg')
    #img=cv2.resize(img,(640,480))
    cv2.imshow('image', img)
    #cv2.imshow('payload', msg.payload)
    key=cv2.waitKey(10000)
    cv2.destroyWindow('image')
    # 按q離開
    if key & 0xFF == ord('q'):
        exit()
    print('image received')
 
#設定Mqtt連線    
client = mqtt.Client()
client.on_connect = on_connect
client.on_message = on_message
client.connect(MqttBroker, MqttPort, 60)
#等候訂閱
client.loop_forever()