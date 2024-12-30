from flask import Flask, request, jsonify 
import csv 
import os 
from datetime import datetime

app = Flask(__name__) 

# Function to create directory and file paths 
def get_file_path(): 
    current_date = datetime.now().strftime("%Y-%m-%d") 
    current_time = datetime.now().strftime("%Hh%M") 
    dir_path = f'./data/{current_date}' 
    
    # Create the directory if it doesn't exist 
    if not os.path.exists(dir_path): 
        os.makedirs(dir_path) 
        
    # Create a new file path with current time 
    file_path = os.path.join(dir_path, f'{current_time}.csv') 
    return file_path

FILE_PATH = get_file_path()
FILE_PATH = './data/2024-12-30/22h41.csv'

@app.route('/') 
def hello_world(): 
    return 'Hello, World!' 

# Function to write data to CSV 
def write_to_csv(data, file_path): 
    try: 
        file_exists = os.path.isfile(file_path) 
        with open(file_path, mode='a', newline='') as file: 
            writer = csv.writer(file) 
            if not file_exists: 
                # Write the header if the file does not exist 
                writer.writerow(['room', 'reading_id', 'temperature', 'humidity', 'gas', 'state', 'timestamp']) 
            for room, readings in data['rooms'].items(): 
                for reading_id, reading in readings['readings'].items(): 
                    writer.writerow([room, reading_id, reading['temperature'], reading['humidity'], reading['gas'], reading['state'], reading['timestamp']]) 
                    print("\033[92mData written to CSV successfully.\033[0m") 
    except Exception as e: 
        print(f"\033[91mError writing to CSV: {e}\033[0m")
        
def read_csv_file(file_path):
    data = {'rooms': {}}
    if os.path.isfile(file_path):
        with open(file_path, mode='r', newline='') as file:
            reader = csv.DictReader(file)
            for row in reader:
                room = row['room']
                reading_id = row['reading_id']
                if room not in data['rooms']:
                    data['rooms'][room] = {'readings': {}}
                # Ensure reading_id is a string to avoid overwriting data
                reading_id = str(reading_id)
                data['rooms'][room]['readings'][reading_id] = {
                    'temperature': float(row['temperature']),
                    'humidity': float(row['humidity']),
                    'gas': float(row['gas']),
                    'state': row['state'].lower() == 'true',
                    'timestamp': float(row['timestamp'])
                }
    else:
        print(f"\033[91mFile not found: {file_path}\033[0m")
    return data

        
        
# Define the API endpoint 
@app.route('/data', methods=['POST']) 
def post_data():
    # file_path = get_file_path() 
    
    if request.is_json: 
        data = request.get_json() 
        write_to_csv(data, FILE_PATH) 
        return jsonify({"message": "Data saved to CSV"}), 200 
    else: 
        return jsonify({"message": "Request must be JSON"}), 400 
    
# Define the API endpoint for getting all data from the CSV file
@app.route('/data', methods=['GET'])
def get_all_data_from_file():
    all_data = read_csv_file(FILE_PATH)
    if all_data['rooms']:
        return jsonify(all_data), 200
    else:
        return jsonify({"message": "No data found"}), 404

    
if __name__ == '__main__': 
    app.run(debug=True)