import requests
import time
import matplotlib.pyplot as plt
import matplotlib.dates as mdates
from datetime import datetime
import argparse
import json

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
            timestamp = f"{datetime.now().strftime('%Y-%m-%d')} {json_data.get('timestamp')}"
            
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
    timestamps = [datetime.strptime(d["timestamp"], "%Y-%m-%d %H:%M:%S") for d in data_list]

    # Find how many temperature values per entry
    num_sensors = len(data_list[0]["temp"])

    plt.figure(figsize=(10, 5))

    # Plot each temperature index as a separate line
    for i in range(num_sensors):
        temps = [d["temp"][i] for d in data_list]
        #plt.plot(timestamps, temps, marker="o", linestyle="-", label=f"temp[{i}]")
        plt.plot(timestamps, temps, linestyle="-", label=f"temp[{i}]")

    plt.title(f"Temperature Data: {data_list[0]['timestamp']} - {data_list[-1]['timestamp']}")
    plt.xlabel("Timestamp")
    plt.ylabel("Temperature")
    #plt.xticks(rotation=45)
    plt.grid(True)
    plt.legend()
    plt.tight_layout()
    ax = plt.gca()
    ax.xaxis.set_major_formatter(mdates.DateFormatter('%a %H:%M'))
    ax.xaxis.set_major_locator(mdates.MinuteLocator(interval=60)) # Ticks every 30 minutes
    plt.gcf().autofmt_xdate() # Automatically rotates and aligns x-axis labels

    plot_file = f"temp_plots_{data_list[0]['timestamp'].replace(' ', '_').replace(':', '').replace('-', '')}.png"
    plt.savefig(plot_file) 

    # --- Now do the humidity data
    plt.figure(figsize=(10, 5))

    # Plot each humidity index as a separate line
    for i in range(num_sensors):
        humidities = [d["humidity"][i] for d in data_list]
        #plt.plot(timestamps, humidities, marker="o", linestyle="-", label=f"humidity[{i}]")
        plt.plot(timestamps, humidities, linestyle="-", label=f"humidity[{i}]")

    plt.title(f"Humidity Data: {data_list[0]['timestamp']} - {data_list[-1]['timestamp']}")
    plt.xlabel("Timestamp")
    plt.ylabel("Humidity")
    #plt.xticks(rotation=45)
    plt.grid(True)
    plt.legend()
    plt.tight_layout()
    ax = plt.gca()
    ax.xaxis.set_major_formatter(mdates.DateFormatter('%a %H:%M'))
    ax.xaxis.set_major_locator(mdates.MinuteLocator(interval=60)) # Ticks every 30 minutes
    plt.gcf().autofmt_xdate() # Automatically rotates and aligns x-axis labels
    
    plot_file = f"humidity_plots_{data_list[0]['timestamp'].replace(' ', '_').replace(':', '').replace('-', '')}.png"
    plt.savefig(plot_file) 

    plt.show()

def read_json_file(filename):
    """Reads a JSON file from disk and returns the Python data structure."""
    try:
        with open(filename, "r", encoding="utf-8") as f:
            data = json.load(f)  # Parse JSON into Python dict/list
        return data
    except FileNotFoundError:
        print(f"Error: The file '{filename}' was not found.")
        return None
    except json.JSONDecodeError as e:
        print(f"Error decoding JSON: {e}")
        return None

def write_json_file(filename, data):
    """Writes a Python data structure to a JSON file on disk."""
    try:
        with open(filename, "w", encoding="utf-8") as f:
            json.dump(data, f, indent=4)  # Pretty-print with indent
        print(f"Data successfully written to '{filename}'")
    except Exception as e:
        print(f"Error writing JSON: {e}")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Collect temp/humidity data from ESP temp server, and plot collected data')
    parser.add_argument('--ip_port', type=str, default="192.168.129.202:8088", help='IP Address and port of the temp server (192.168.129.202:8088)')
    parser.add_argument('--iters', type=int, default=5, help='Number of iterations of data to collect')
    parser.add_argument('--delay', type=int, default=5, help='Delay (sec) between iteration')
    parser.add_argument('--plot_data_file', type=str, default='', help='Instead of running normally, simply plot the previously saved data JSON file')
    args = parser.parse_args()
    
    if len(args.plot_data_file) > 0:
        print(f"Instead of running normally, plotting data from {args.plot_data_file} instead...")
        collected_data = read_json_file(args.plot_data_file)
    
    else:
        server_url = f"http://{args.ip_port}"
        collected_data = fetch_temperature_data(server_url, iterations=args.iters, delay=args.delay)

        print("\nFinal Collected Data:")
        for entry in collected_data:
            print(entry)
        
        output_file = f"{collected_data[0]['timestamp'].replace(' ', '_').replace(':', '').replace('-', '')}_collected_data.json"
        write_json_file(output_file, collected_data)

    # Plot the collected data
    plot_data(collected_data)