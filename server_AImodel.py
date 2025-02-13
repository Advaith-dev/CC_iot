from flask import Flask, request, jsonify
import os
import cv2
import numpy as np
from ultralytics import YOLO  # Assuming YOLOv8 is used
from collections import Counter

import os, time
from influxdb_client_3 import InfluxDBClient3, Point

token = "Kn7SDJ8Ai0nEFqovtToZ5v4v-Q5NEVTNNtCnFjeOtaBUxLm2U43LuVpS1DYJIQulJQzBU9lWZ7cDov1eGDDEoQ=="
org = "Smartbin"
host = "https://eu-central-1-1.aws.cloud2.influxdata.com"

client = InfluxDBClient3(host=host, token=token, org=org)

database="Cashcrow"

app = Flask(__name__)

# Initialize YOLO model
model = YOLO("best.pt")  # Replace with your trained YOLO model path

# Folder to temporarily save images
os.makedirs("received_images", exist_ok=True)
received_images = []
predictions = []

@app.route("/check", methods=['GET'])
def msg():
    return "Hello, Im working"

@app.route('/classify', methods=['POST'])
def classify_image():
    global received_images, predictions

    # Read raw binary data from the request
    image_data = request.data

    # Check if the `is_last` header exists
    image_count = request.headers.get('imgCount')
    print("Image Number:", image_count)

    # Save the incoming image temporarily
    image_path = f"received_images/image_{len(received_images)}.jpg"
    with open(image_path, "wb") as f:
        f.write(image_data)

    # Append the image path to the list
    received_images.append(image_path)

    # Run YOLO inference on the received image
    results = model.predict(image_path, conf=0.6, imgsz=640)
    if len(results[0].boxes) > 0:
        # Take the class with the highest confidence score
        #best_prediction = results[0].boxes[0]
        #class_name = model.names[best_prediction.cls]  # Get class name
        #predictions.append(class_name)
        detected_objects = [model.names[int(cls)] for cls in results[0].boxes.cls]
        predictions.append(detected_objects[0])
        conf = results[0].boxes.conf
        print("Detected objects:", detected_objects[0])
    else:
        # No detections
        predictions.append("Other")

    # If this is the last image, process and respond
    if int(image_count) == 4:
        # Count the most common class from predictions
        Id = request.headers.get('Unit-ID')
        print("\nLast image received.\nPrediction Array: ", predictions)
        most_common_class = Counter(predictions).most_common(1)[0][0]
        print("Most common class :",(most_common_class))
        vMap = {"Plastic_bottle": "0", "Plastic_container": "0", "Plastic_wrapper": "0", 
                "Paper_box": "1", "Paper_tetra": "1", "Paper_white": "1", 
                "Metal_Can":"2", "Wood" : "2","Other":"2"}
        
        Waste_material = vMap[most_common_class]
        binValue = vMap[most_common_class]
        print("Bin Value:", binValue)

        # Cleanup: Delete all received images
        #for img_path in received_images:
         #   os.remove(img_path)

        # Reset global variables
        received_images = []
        predictions = []

        # #database write
        # database="Cashcrow"
        # data = {
        #     "point1": {
        #     "data":"Waste_Material",
        #     "Waste_type": most_common_class,
        #     "bin_Id":Id,
        #     },
        # }

        # for key in data:
        #     point = (
        #     Point("Bin data")
        #     .tag("Bin id", data[key]["bin_Id"])
        #     .field(data[key]["data"],data[key]["Waste_type"])
        # )


        # client.write(database=database, record=point)
        # print("DB update Complete.")

        # Respond with the most probable class
        return binValue, 200
    

    # Acknowledge receipt if not the last image
    return jsonify({"status": "image received"})
    

if __name__ == '__main__':
    app.run(host='0.0.0.0')



