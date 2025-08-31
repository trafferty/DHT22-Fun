import requests
import time
import matplotlib.pyplot as plt
from datetime import datetime

def fetch_temperature_data(base_url, iterations=5, delay=2):
    """
    Fetch temperature data from the server and store it in a list.

    :param base_url: The base URL of the server (e.g. "http://localhost:5000")
    :param iterations: How many times to poll the endpoint
    :param delay: Delay (seconds) between requests
    :return: List of dictionaries with 'timestamp' and multiple 'tempX' fields
    """
    data_list = []

    print(f"Connecting to {base_url} to collect data.  {iterations} iters")
    for i in range(iterations):
        try:
            response = requests.get(f"{base_url}/get_data", timeout=5)            
            response.raise_for_status()  # raise error for bad responses
            json_data = response.json()

            # Extract timestamp and temp array
            timestamp = json_data.get("timestamp")
            temps = json_data.get("temp")
            humidities = json_data.get("humidity")

            if timestamp is not None and isinstance(temps, list):
                entry = {"timestamp": timestamp, "temp": temps,  "humidity": humidities}
                data_list.append(entry)
                print(f"Fetched: {entry}")
            else:
                print("Invalid response format:", json_data)

        except requests.exceptions.Timeout:
            print("The request timed out.")
        except requests.RequestException as e:
            print("Request failed:", e)

        time.sleep(delay)

    return data_list


def plot_data(data_list):
    """
    Plot temperature series vs timestamp using matplotlib.
    Each element in the 'temp' array gets its own line.
    """
    if not data_list:
        print("No data to plot.")
        return

    # Convert timestamps into datetime objects
    #timestamps = [datetime.fromisoformat(d["timestamp"]) for d in data_list]
    timestamps = [datetime.strptime(d["timestamp"], "%H:%M:%S") for d in data_list]

    # Find how many temperature values per entry
    num_sensors = len(data_list[0]["temp"])

    plt.figure(figsize=(10, 5))

    # Plot each temperature index as a separate line
    for i in range(num_sensors):
        temps = [d["temp"][i] for d in data_list]
        plt.plot(timestamps, temps, marker="o", linestyle="-", label=f"temp[{i}]")

    plt.title("Temperature Data Over Time")
    plt.xlabel("Timestamp")
    plt.ylabel("Temperature")
    plt.xticks(rotation=45)
    plt.grid(True)
    plt.legend()
    plt.tight_layout()

    plt.figure(figsize=(10, 5))

    # Plot each humidity index as a separate line
    for i in range(num_sensors):
        humidities = [d["humidity"][i] for d in data_list]
        plt.plot(timestamps, humidities, marker="o", linestyle="-", label=f"humidity[{i}]")

    plt.title("Humidity Data Over Time")
    plt.xlabel("Timestamp")
    plt.ylabel("Humidity")
    plt.xticks(rotation=45)
    plt.grid(True)
    plt.legend()
    plt.tight_layout()
    plt.show()


if __name__ == "__main__":
    # Example usage: change to your server URL
    server_url = "http://192.168.129.205:8088"
    collected_data = fetch_temperature_data(server_url, iterations=60, delay=60)

    print("\nFinal Collected Data:")
    for entry in collected_data:
        print(entry)

    # Plot the collected data
    plot_data(collected_data)
