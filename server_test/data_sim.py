from flask import Flask, jsonify
import random
from datetime import datetime

app = Flask(__name__)

@app.route('/get_data', methods=['GET'])
def get_temp():
    # Generate a random temperature between -10.0 and 40.0 °C
    temp1 = round(random.uniform(-10.0, 10.0), 2)
    temp2 = round(random.uniform( 10.1, 20.0), 2)
    temp3 = round(random.uniform( 20.1, 40.0), 2)
    humidity1 = round(random.uniform( 40.0, 60.0), 2)
    humidity2 = round(random.uniform( 40.0, 60.0), 2)
    humidity3 = round(random.uniform( 40.0, 60.0), 2)
    # Current UTC timestamp in ISO 8601 format
    #timestamp = datetime.now(datetime.timezone.utc) #+ "Z"
    timestamp = datetime.now().strftime("%Y-%m-%dT%H:%M:%S")
    return jsonify({
        "temp1": temp1,
        "temp2": temp2,
        "temp3": temp3,
        "humidity1": humidity1,
        "humidity2": humidity2,
        "humidity3": humidity3,
        "timestamp": timestamp
    })

if __name__ == '__main__':
    app.run(host="0.0.0.0", port=8088, debug=True)