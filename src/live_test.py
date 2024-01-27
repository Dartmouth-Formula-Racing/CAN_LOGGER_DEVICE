import streamlit as st
import pandas as pd
import time
import datetime
import numpy as np
import CANverter as canv
import serial
import subprocess
import sys


def generate_data(canverter : canv.CANverter):
    file_position = 0
    while True:
        try:
            with open('can_message.txt', 'r') as f:
                f.seek(file_position)  # fast forward beyond content read previously
                for line in f:
                    try:
                        decoded_line = line[:-1]
                        df = canverter.decode_message_stream(decoded_line)
                        # print('before yield')
                        yield df
                    except Exception as ex:
                        # print(ex)
                        pass
                file_position = f.tell()  # store position at which to resume
        except Exception as ex:
            # print(ex)
            pass

@st.cache_resource
def get_canverter(dbc_file_path):
    # print('getting canverter')
    canverter = canv.CANverter(dbc_file_path)
    df = canverter.log_to_dataframe("/Users/jason/Documents/Dartmouth/W24/ENGS 90/git/ENGS89-90-CAN-Decoder/data/CAN_00012.log")
    return canverter

@st.cache_data
def start_listener(temp):
    print('starting listener')
    # p = subprocess.Popen(["python", "/Users/jason/Documents/Dartmouth/W24/ENGS 90/git/ENGS89-90-CAN-Decoder/src/USB_listener.py", '&']) 
    return 0

def main():
    # print('starting main')
    st.title("Streaming Data into Streamlit App")
    canverter = get_canverter('/Users/jason/Documents/Dartmouth/W24/ENGS 90/git/ENGS89-90-CAN-Decoder/dbc/test.dbc')
    start_listener(0)

    column_names = ['Time ms', 'Mod A Temp (C)', 'Mod B Temp (C)', 'Mod C Temp (C)',
       'Gate Driver Board Temp (C)', 'Control Board Temp (C)',
       'RTD 1 Temp (C)', 'RTD 2 Temp (C)', 'RTD 3 Temp (C)', 'RTD 4 Temp (C)',
       'RTD 5 Temp (C)', 'Motor Temp (C)', 'Torque Shudder (N*m)',
       'Motor Angle Electrical (degrees)', 'Motor Speed (rpm)',
       'Electrical Output Freq (Hz)', 'Delta Resolver Filtered (degrees)',
       'GPS Latitude (degrees)', 'GPS Longitude (degrees)', 'GPS Speed (mph)',
       'GPS Altitude (ft)', 'GPS TrueCourse (degrees)',
       'GPS SatellitesInUse (satellites)', 'GPS Valid (OK(1)/N(2))',
       'GPS Valid (OK(1)/N(2))', 'UTC Year (year)', 'UTC Month (month)',
       'UTC Day (day)', 'UTC Hour (hour)', 'UTC Minute (min)',
       'UTC Second (sec)', 'X Axis Acceleration (g)',
       'Y Axis Acceleration (g)', 'Z Axis Acceleration (g)',
       'X Axis YawRate (deg/s)', 'Y Axis YawRate (deg/s)',
       'Z Axis YawRate (deg/s)']

    # Create a master DataFrame to retain all data
    df = pd.DataFrame(columns=column_names)

    # Allow the user to select columns to display
    column_names.remove('Time ms')
    selected_columns = st.multiselect("Select Columns to Display", column_names)

    # Add more space using Markdown
    st.markdown("&nbsp;\n\n&nbsp;\n\n&nbsp;")  # HTML non-breaking space

    # Create a line chart for streaming data
    chart = st.empty()

    # Stream data into the app
    data_stream = generate_data(canverter)
    # print('past stream')
    while True:
        new_data = next(data_stream)
        df = pd.concat([df, new_data]).iloc[-1000:]

        # print(df.head())

        if (selected_columns != []):
            chart.line_chart(df, x='Time ms', y=selected_columns, width=0, height=300)

if __name__ == "__main__":
    main()