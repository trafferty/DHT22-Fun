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

    for i in range(iterations):
        try:
            response = requests.get(f"{base_url}/get_data")
            response.raise_for_status()  # raise error for bad responses
            json_data = response.json()

            # Extract timestamp
            timestamp = json_data.get("timestamp")

            # Extract all fields that start with 'temp'
            temps      = {k: v for k, v in json_data.items() if k.startswith("temp")}
            humidities = {k: v for k, v in json_data.items() if k.startswith("hum")}

            if timestamp and temps:
                entry = {"timestamp": timestamp, **temps, **humidities}
                data_list.append(entry)
                print(f"Fetched: {entry}")
            else:
                print("Invalid response format:", json_data)

        except requests.RequestException as e:
            print("Request failed:", e)

        time.sleep(delay)

    return data_list


def plot_temperature_data(data_list):
    """
    Plot multiple temperature series vs timestamp using matplotlib.
    """
    if not data_list:
        print("No data to plot.")
        return

    timestamps = [datetime.fromisoformat(d["timestamp"]) for d in data_list]

    # Find all temp keys
    temp_keys = [k for k in data_list[0].keys() if k.startswith("temp")]

    plt.figure(figsize=(10, 5))

    for key in temp_keys:
        temps = [d.get(key) for d in data_list]
        plt.plot(timestamps, temps, marker="o", linestyle="-", label=key)

    plt.title("Temperature Over Time")
    plt.xlabel("Timestamp")
    plt.ylabel("Temperature")
    plt.xticks(rotation=45)
    plt.grid(True)
    plt.legend()
    plt.tight_layout()
    plt.show()

def plot_humidity_data(data_list):
    """
    Plot multiple humidity series vs timestamp using matplotlib.
    """
    if not data_list:
        print("No data to plot.")
        return

    timestamps = [datetime.fromisoformat(d["timestamp"]) for d in data_list]

    # Find all temp keys
    hum_keys = [k for k in data_list[0].keys() if k.startswith("hum")]

    plt.figure(figsize=(10, 5))

    for key in hum_keys:
        humidities = [d.get(key) for d in data_list]
        plt.plot(timestamps, humidities, marker="o", linestyle="-", label=key)

    plt.title("Humidity Over Time")
    plt.xlabel("Timestamp")
    plt.ylabel("Humidity")
    plt.xticks(rotation=45)
    plt.grid(True)
    plt.legend()
    plt.tight_layout()
    plt.show()


if __name__ == "__main__":
    # Example usage: change to your server URL
    server_url = "http://localhost:8088"
    collected_data = fetch_temperature_data(server_url, iterations=10, delay=2)

    print("\nFinal Collected Data:")
    for entry in collected_data:
        print(entry)

    # Plot the collected data
    plot_temperature_data(collected_data)
    plot_humidity_data(collected_data)
