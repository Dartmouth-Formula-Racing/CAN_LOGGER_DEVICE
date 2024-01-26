import cantools
from tqdm import tqdm
import re
from tkinter import Tk
from tkinter.filedialog import askopenfilename
from tkinter.filedialog import asksaveasfilename
import pandas as pd

class CANverter():
    #CONSTANTS
    SOCKET_CAN_LINE_PATTERN = re.compile(r"\((\d+)\)\s+[^\s]+\s+([0-9A-F#]{3}|[0-9A-F#]{8})#([0-9A-F]+)")
    DPS_BASE = 3
    LOGGING_BASE = 1
    
    # def __create_dbc_object(self, dbcFileName : str):
    #     return cantools.database.load_file(dbcFileName)

    def __init__(self, dbc_file_path : str):
        self.signalList = ['Time']
        self.displaySignalList = ['Time']
        self.signalUnitList = ['ms']
        self.signalMinList = [None]
        self.signalMaxList = [None]
        self.dpsList = [3]
        self.dbc = cantools.database.load_file(dbc_file_path)

    def get_encoded_pattern(self, row : str):
        #Get tokens
        tokens = CANverter.SOCKET_CAN_LINE_PATTERN.search(row).groups()
        #return tokens in correct format
        timestamp = int(tokens[0])
        identifier = int(tokens[1],16)
        dataPacket = bytearray.fromhex(tokens[2])

        return (timestamp, identifier, dataPacket)


    def get_decoded_values(self, identifier, data, aggregatedValues, isStream):
        decodedMessage = self.dbc.decode_message(identifier, data, decode_choices=False)
        for (signalName, signalValue) in decodedMessage.items():
            if signalName in self.signalList:
                signalIndex = self.signalList.index(signalName)
                signalMin = self.signalMinList[signalIndex]
                signalMax = self.signalMaxList[signalIndex]
                
                if  ((signalMin == None or signalValue > signalMin) and (signalMax == None or signalValue < signalMax)):
                    dps = self.dpsList[signalIndex]
                    if dps != None:
                        try:
                            signalValue = round(float(signalValue), dps)
                            if int(signalValue) == float(signalValue):
                                signalValue = int(signalValue)
                        except:
                            pass
                    if isStream:
                        aggregatedValues[signalIndex] = signalValue
                    else:
                        aggregatedValues[signalIndex].append(signalValue)
                    
    def get_identifier_list(self):
        identifierList = []

        for dbcMessage in self.dbc.messages:
            messageList = str(dbcMessage).split(',')
            identifierList.append(int(messageList[1],0))
        identifierList.sort()

        return identifierList

    def log_to_dataframe(self, logFileName : str):
        

        with open (logFileName, "r",encoding="utf8") as logFile:
            numLines = len(logFile.readlines())
        logFile.close()

        with open (logFileName, "r",encoding="utf8") as logFile:
            identifierList = self.get_identifier_list()

            for identifier in identifierList:
                frameID = self.dbc.get_message_by_frame_id(identifier)
                signalSet = frameID.signals        

                if len(signalSet) > 0:
                    for signal in signalSet:
                        if signal.is_multiplexer == False:
                            signalName    = str(signal.name)
                            modSignalName = str(signal.name).replace("_"," ")
                            signalUnit    = signal.unit
                            signalComment = signal.comment
                            signalMinimum = signal.minimum
                            signalMaximum = signal.maximum
                            log = CANverter.LOGGING_BASE
                            try:
                                log = int(re.findall("LOG = (d{1})",signalComment)[0])
                            except: 
                                pass

                            if log >=1:
                                self.signalList.append(signalName)
                                self.displaySignalList.append(modSignalName)
                                self.signalMinList.append(signalMinimum)
                                self.signalMaxList.append(signalMaximum)
                                if signalUnit != None:
                                    self.signalUnitList.append("("+signalUnit+")")
                                else:
                                    self.signalUnitList.append('')
                                try:
                                    dps = int(re.findall("DPS = (\d{2}|\d{1})",signalComment)[0])
                                    self.dpsList(dps)
                                except:
                                    self.dpsList.append(CANverter.DPS_BASE)

            for i in range(len(self.signalList)) :
                self.displaySignalList[i] += " " + self.signalUnitList[i]

            valueRows = []
            aggregatedValuesList = [''] * len(self.signalList)
            valuesList = [ [] for _ in range(len(self.signalList)) ]
            
            (lastTimestamp, identifier, data) = self.get_encoded_pattern(logFile.readline())
            self.get_decoded_values(identifier, data, valuesList, False)

            for row in tqdm(logFile,desc= "Lines", total = numLines,unit = " Lines"):
                try:
                    (timestamp, identifier, data) = self.get_encoded_pattern(row)

                    if (lastTimestamp != timestamp):
                        for i, value in enumerate(valuesList):
                            if len(value) > 0:
                                try:
                                    averageValue = float(sum(value)/len(value))
                                except:
                                    averageValue = float(value[-1])
                                if self.dpsList[i] != 'None':
                                    try:
                                        averageValue = round(averageValue, self.dpsList[i])
                                        if int(averageValue) == averageValue:
                                            averageValue = int(averageValue)
                                    except:
                                        pass
                                aggregatedValuesList[i] = averageValue
                        aggregatedValuesList[0] = str("%d" %(lastTimestamp))
                        valueRows.append(aggregatedValuesList.copy())
                        lastTimestamp = timestamp
                        valuesList = [ [] for _ in range(len(self.signalList)) ]
                        aggregatedValuesList = [''] * len(valuesList)

                    self.get_decoded_values(identifier, data, valuesList, False)
                    
                except:
                    pass
                    # print("invalidated line observed: '%s'"% (row[:-1]))
        logFile.close()
        return pd.DataFrame(valueRows, columns = self.displaySignalList) #, dtype = float)


    def decode_message_stream(self, socketCANLine):
        valuesList = [ 0 for _ in range(len(self.signalList)) ]
        (timestamp, identifier, data) = self.get_encoded_pattern(socketCANLine)
        self.get_decoded_values(identifier, data, valuesList, True)
        valuesList[0] = timestamp
        return pd.DataFrame([valuesList], columns = self.displaySignalList, dtype = float)

if __name__ == "__main__":
    Tk().withdraw() # we don't want a full GUI, so keep the root window from appearing

    canverter = CANverter("/Users/jason/Documents/Dartmouth/W24/ENGS 90/git/ENGS89-90-CAN-Decoder/dbc/test.dbc")

    df = canverter.log_to_dataframe("/Users/jason/Documents/Dartmouth/W24/ENGS 90/git/ENGS89-90-CAN-Decoder/data/CAN_00012.log")

    df.to_csv( "/Users/jason/Documents/Dartmouth/W24/ENGS 90/git/ENGS89-90-CAN-Decoder/data/SDCardDecodedAll.csv")
    dfsingle = canverter.decode_message_stream("(0000000331) X 000A0003#0302005010220000")
    columnNames = dfsingle.columns
    print(columnNames)
    # print(dfsingle)

    #askopenfilename(title = "Select Log File",filetypes = (("LOG Files","*.log"),("all files","*.*"))) 
    #askopenfilename(title = "Select DBC File",filetypes = (("DBC Files","*.dbc"),("all files","*.*"))) 
    #asksaveasfilename(title = "Save Exported CSV File", filetypes = (("CSV Files","*.csv"),("all files","*.*")))

    # dbc = create_dbc_object("/Users/jason/Documents/Dartmouth/W24/ENGS 90/git/ENGS89-90-CAN-Decoder/dbc/test.dbc")
    # df = log_to_dataframe(dbc, "/Users/jason/Documents/Dartmouth/W24/ENGS 90/git/ENGS89-90-CAN-Decoder/data/CAN_00012.log")
    # df.to_csv( "/Users/jason/Documents/Dartmouth/W24/ENGS 90/git/ENGS89-90-CAN-Decoder/data/SDCardDecodedAll.csv")
    # dfsingle = decode_message_stream(dbc, "(0000000331) X 000A0003#0302005010220000")
    # print(dfsingle)
