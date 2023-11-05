import csv

inverterEncodedData = open('DFRInverterEncodedData.csv', "r")
socketCanRawData = open('SocketCANData.log', "w")

csvReader = csv.reader(inverterEncodedData)
csvWriter = csv.writer(socketCanRawData, delimiter=" ")

for row in csvReader:
    signalValues = row[0].split('\t')

    if len(signalValues[1]) == 4:
        # Extracting Timestamp in (ex. (244.03))
        timestamp = "("+signalValues[0]+ ".0)"

        #Extracting CAN ID
        canID = signalValues[1][1:4].upper()

        #RawSocketCAN uses an arbitrary string could store useful info
        arbitraryValue = "X"

        #All data bytes concatenated together
        dataHexString = ""

        for i in range(5, len(signalValues)):
            if len(signalValues[i]) == 1:
                dataHexString = dataHexString +  "0" + signalValues[i]
            else:
                dataHexString = dataHexString +  signalValues[i]

        csvWriter.writerow([timestamp, arbitraryValue, canID+"#"+dataHexString])

inverterEncodedData.close()
socketCanRawData.close()