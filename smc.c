/*
 * Apple System Management Control (SMC) Tool 
 * Copyright (C) 2006 devnull
 * Copyright 2007 iSlayer & Distorted Vista.
 * Copyright (C) 2009 FreeGGGroup bootblack http://d.hatena.ne.jp/bootblack/20090610
 * Copyright (C) 2013 Y.Okamura http://informationsea.info/
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <IOKit/IOKitLib.h>

#include "smc.h"

static io_connect_t conn;

/* This list copied from iStat pro source code */
static SMCTempKeyList_t temp_keys[] = {
    {"Mem Controller", "Tm0P"},
    {"Mem Bank A1", "TM0P"},
    {"Mem Bank A2", "TM1P"},
    {"Mem Bank A3", "TM2P"},
    {"Mem Bank A4", "TM3P"},
    {"Mem Bank A5", "TM4P"},
    {"Mem Bank A6", "TM5P"},
    {"Mem Bank A7", "TM6P"},
    {"Mem Bank A8", "TM7P"},
    {"Mem Bank B1", "TM8P"},
    {"Mem Bank B2", "TM9P"},
    {"Mem Bank B3", "TMAP"},
    {"Mem Bank B4", "TMBP"},
    {"Mem Bank B5", "TMCP"},
    {"Mem Bank B6", "TMDP"},
    {"Mem Bank B7", "TMEP"},
    {"Mem Bank B8", "TMFP"},
    {"Mem module A1", "TM0S"},
    {"Mem module A2", "TM1S"},
    {"Mem module A3", "TM2S"},
    {"Mem module A4", "TM3S"},
    {"Mem module A5", "TM4S"},
    {"Mem module A6", "TM5S"},
    {"Mem module A7", "TM6S"},
    {"Mem module A8", "TM7S"},
    {"Mem module B1", "TM8S"},
    {"Mem module B2", "TM9S"},
    {"Mem module B3", "TMAS"},
    {"Mem module B4", "TMBS"},
    {"Mem module B5", "TMCS"},
    {"Mem module B6", "TMDS"},
    {"Mem module B7", "TMES"},
    {"Mem module B8", "TMFS"},
    {"CPU A", "TC0H"},
    {"CPU A", "TC0D"},
    {"CPU B", "TC1D"},
    {"CPU C", "TC2D"},
    {"CPU D", "TC3D"},
    {"CPU A", "TCAH"},
    {"CPU B", "TCBH"},
    {"CPU C", "TCCH"},
    {"CPU D", "TCDH"},
    {"GPU", "TG0P"},
    {"Ambient", "TA0P"},
    {"HD Bay 1", "TA0P"},
    {"HD Bay 2", "TH1P"},
    {"HD Bay 3", "TH2P"},
    {"HD Bay 4", "TH3P"},
    {"Optical Drive", "TO0P"},
    {"Heatsink A", "Th0H"},
    {"Heatsink B", "Th1H"},
    {"Heatsink C", "Th2H"},
    {"GPU Diode", "TG0D"},
    {"GPU Heatsink", "TG0H"},
    {"GPU Heatsink 2", "TG1H"},
    {"Power supply 2", "Tp1C"},
    {"Power supply 1", "Tp0C"},
    {"Power supply 1", "Tp0P"},
    {"Enclosure Base", "TB0T"},
    {"Enclosure Base 2", "TB1T"},
    {"Enclosure Base 3", "TB2T"},
    {"Enclosure Base 4", "TB3T"},
    {"Northbridge 1", "TN0P"},
    {"Northbridge 2", "TN1P"},
    {"Northbridge", "TN0H"},
    {"Expansion Slots", "TS0C"},
    {"Airport Card", "TW0P"},
    {"PCI Slot 1 Pos 1", "TA0S"},
    {"PCI Slot 1 Pos 2", "TA1S"},
    {"PCI Slot 2 Pos 1", "TA2S"},
    {"PCI Slot 2 Pos 2", "TA3S"},
    {"Ambient 2", "TA1P"},
    {"Power supply 2", "Tp1P"},
    {"Power supply 3", "Tp2P"},
    {"Power supply 4", "Tp3P"},
    {"Power supply 5", "Tp4P"},
    {"Power supply 6", "Tp5P"},
};


UInt32 _strtoul(char *str, int size, int base)
{
    UInt32 total = 0;
    int i;

    for (i = 0; i < size; i++)
        {
            if (base == 16)
                total += str[i] << (size - 1 - i) * 8;
            else
                total += (unsigned char) (str[i] << (size - 1 - i) * 8);
        }
    return total;
}

void _ultostr(char *str, UInt32 val)
{
    str[0] = '\0';
    sprintf(str, "%c%c%c%c", 
            (unsigned int) val >> 24,
            (unsigned int) val >> 16,
            (unsigned int) val >> 8,
            (unsigned int) val);
}

float _strtof(char *str, int size, int e)
{
    float total = 0;
    int i;

    for (i = 0; i < size; i++)
        {
            if (i == (size - 1))
                total += (str[i] & 0xff) >> e;
            else
                total += str[i] << (size - 1 - i) * (8 - e);
        }

    return total;
}

void printFPE2(SMCVal_t val)
{
    /* FIXME: This decode is incomplete, last 2 bits are dropped */

    printf("%.0f ", _strtof(val.bytes, val.dataSize, 2));
}

void printUInt(SMCVal_t val)
{
    printf("%u ", (unsigned int) _strtoul(val.bytes, val.dataSize, 10));
}

void printBytesHex(SMCVal_t val)
{
    int i;

    printf("(bytes");
    for (i = 0; i < val.dataSize; i++)
        printf(" %02x", (unsigned char) val.bytes[i]);
    printf(")\n");
}

void printVal(SMCVal_t val)
{
    printf("  %-4s  [%-4s]  ", val.key, val.dataType);
    if (val.dataSize > 0)
        {
            if ((strcmp(val.dataType, DATATYPE_UINT8) == 0) ||
                (strcmp(val.dataType, DATATYPE_UINT16) == 0) ||
                (strcmp(val.dataType, DATATYPE_UINT32) == 0))
                printUInt(val);
            else if (strcmp(val.dataType, DATATYPE_FPE2) == 0)
                printFPE2(val);

            printBytesHex(val);
        }
    else
        {
            printf("no data\n");
        }
}

kern_return_t SMCOpen(io_connect_t *conn)
{
    kern_return_t result;
    mach_port_t   masterPort;
    io_iterator_t iterator;
    io_object_t   device;

    result = IOMasterPort(MACH_PORT_NULL, &masterPort);

    CFMutableDictionaryRef matchingDictionary = IOServiceMatching("AppleSMC");
    result = IOServiceGetMatchingServices(masterPort, matchingDictionary, &iterator);
    if (result != kIOReturnSuccess)
        {
            printf("Error: IOServiceGetMatchingServices() = %08x\n", result);
            return 1;
        }

    device = IOIteratorNext(iterator);
    IOObjectRelease(iterator);
    if (device == 0)
        {
            printf("Error: no SMC found\n");
            return 1;
        }

    result = IOServiceOpen(device, mach_task_self(), 0, conn);
    IOObjectRelease(device);
    if (result != kIOReturnSuccess)
        {
            printf("Error: IOServiceOpen() = %08x\n", result);
            return 1;
        }

    return kIOReturnSuccess;
}

kern_return_t SMCClose(io_connect_t conn)
{
    return IOServiceClose(conn);
}


kern_return_t SMCCall(int index, SMCKeyData_t *inputStructure, SMCKeyData_t *outputStructure)
{
#if __LP64__
    size_t        structureInputSize;
    size_t        structureOutputSize;
#else
    IOItemCount   structureInputSize;
    IOByteCount   structureOutputSize;
#endif

    structureInputSize = sizeof(SMCKeyData_t);
    structureOutputSize = sizeof(SMCKeyData_t);

#if MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_5    
    return IOConnectCallStructMethod(
        conn,
        index,
        inputStructure,
        structureInputSize,
        outputStructure,
        &structureOutputSize
        );
#else
    return IOConnectMethodStructureIStructureO(
        conn,
        index,
        structureInputSize,
        &structureOutputSize,
        inputStructure,
        outputStructure
        );
#endif
}

kern_return_t SMCReadKey(UInt32Char_t key, SMCVal_t *val)
{
    kern_return_t result;
    SMCKeyData_t  inputStructure;
    SMCKeyData_t  outputStructure;

    memset(&inputStructure, 0, sizeof(SMCKeyData_t));
    memset(&outputStructure, 0, sizeof(SMCKeyData_t));
    memset(val, 0, sizeof(SMCVal_t));

    inputStructure.key = _strtoul(key, 4, 16);
    inputStructure.data8 = SMC_CMD_READ_KEYINFO;    

    result = SMCCall(KERNEL_INDEX_SMC, &inputStructure, &outputStructure);
    if (result != kIOReturnSuccess)
        return result;

    val->dataSize = outputStructure.keyInfo.dataSize;
    _ultostr(val->dataType, outputStructure.keyInfo.dataType);
    inputStructure.keyInfo.dataSize = val->dataSize;
    inputStructure.data8 = SMC_CMD_READ_BYTES;

    result = SMCCall(KERNEL_INDEX_SMC, &inputStructure, &outputStructure);
    if (result != kIOReturnSuccess)
        return result;

    memcpy(val->bytes, outputStructure.bytes, sizeof(outputStructure.bytes));

    return kIOReturnSuccess;
}

kern_return_t SMCWriteKey(SMCVal_t writeVal)
{
    kern_return_t result;
    SMCKeyData_t  inputStructure;
    SMCKeyData_t  outputStructure;

    SMCVal_t      readVal;

    result = SMCReadKey(writeVal.key, &readVal);
    if (result != kIOReturnSuccess) 
        return result;

    if (readVal.dataSize != writeVal.dataSize)
        return kIOReturnError;

    memset(&inputStructure, 0, sizeof(SMCKeyData_t));
    memset(&outputStructure, 0, sizeof(SMCKeyData_t));

    inputStructure.key = _strtoul(writeVal.key, 4, 16);
    inputStructure.data8 = SMC_CMD_WRITE_BYTES;    
    inputStructure.keyInfo.dataSize = writeVal.dataSize;
    memcpy(inputStructure.bytes, writeVal.bytes, sizeof(writeVal.bytes));

    result = SMCCall(KERNEL_INDEX_SMC, &inputStructure, &outputStructure);
    if (result != kIOReturnSuccess)
        return result;
 
    return kIOReturnSuccess;
}

UInt32 SMCReadIndexCount(void)
{
    SMCVal_t val;

    SMCReadKey("#KEY", &val);
    return _strtoul(val.bytes, val.dataSize, 10);
}

kern_return_t SMCPrintAll(void)
{
    kern_return_t result;
    SMCKeyData_t  inputStructure;
    SMCKeyData_t  outputStructure;

    int           totalKeys, i;
    UInt32Char_t  key;
    SMCVal_t      val;

    i = 0;
    do{
        memset(&inputStructure, 0, sizeof(SMCKeyData_t));
        memset(&outputStructure, 0, sizeof(SMCKeyData_t));
        memset(&val, 0, sizeof(SMCVal_t));

        inputStructure.data8 = SMC_CMD_READ_INDEX;
        inputStructure.data32 = i;

        result = SMCCall(KERNEL_INDEX_SMC, &inputStructure, &outputStructure);
        if (result != kIOReturnSuccess)
            continue;

        _ultostr(key, outputStructure.key); 

        result = SMCReadKey(key, &val);
        
        if (val.dataSize == 0)
            break;
        
        strncpy(val.key, key, 4);
        printVal(val);
        i++;
    } while(1);

    return kIOReturnSuccess;
}

kern_return_t SMCPrintFans(void)
{
    kern_return_t result;
    SMCVal_t      val;
    UInt32Char_t  key;
    int           totalFans, i;

    result = SMCReadKey("FNum", &val);
    if (result != kIOReturnSuccess)
        return kIOReturnError;

    totalFans = _strtoul(val.bytes, val.dataSize, 10); 
    printf("Total fans in system: %d\n", totalFans);

    for (i = 0; i < totalFans; i++)
        {
            printf("\nFan #%d:\n", i);
            sprintf(key, "F%dAc", i); 
            SMCReadKey(key, &val); 
            printf("    Actual speed : %.0f\n", _strtof(val.bytes, val.dataSize, 2));
            sprintf(key, "F%dMn", i);   
            SMCReadKey(key, &val);
            printf("    Minimum speed: %.0f\n", _strtof(val.bytes, val.dataSize, 2));
            sprintf(key, "F%dMx", i);   
            SMCReadKey(key, &val);
            printf("    Maximum speed: %.0f\n", _strtof(val.bytes, val.dataSize, 2));
            sprintf(key, "F%dSf", i);   
            SMCReadKey(key, &val);
            printf("    Safe speed   : %.0f\n", _strtof(val.bytes, val.dataSize, 2));
            sprintf(key, "F%dTg", i);   
            SMCReadKey(key, &val);
            printf("    Target speed : %.0f\n", _strtof(val.bytes, val.dataSize, 2));
            SMCReadKey("FS! ", &val);
            if ((_strtoul(val.bytes, 2, 16) & (1 << i)) == 0)
                printf("    Mode         : auto\n"); 
            else
                printf("    Mode         : forced\n");
        
            double temp;
            temp = SMCGetTemperature(SMC_KEY_CPU_TEMP);
            printf("    Temp         = %g\n", temp);
        
            temp = SMCGetTemperature("TB0T");
            printf("    Temp TB0T        = %g\n", temp);
            temp = SMCGetTemperature("TC0D");
            printf("    Temp TC0D        = %g\n", temp);
            temp = SMCGetTemperature("TC0P");
            printf("    Temp TC0P        = %g\n", temp);
            temp = SMCGetTemperature("TM0P");
            printf("    Temp TM0P        = %g\n", temp);
            temp = SMCGetTemperature("TN0P");
            printf("    Temp TN0P        = %g\n", temp);
            temp = SMCGetTemperature("Th0H");
            printf("    Temp Th0H        = %g\n", temp);
            temp = SMCGetTemperature("Ts0P");
            printf("    Temp Ts0P        = %g\n", temp);
            temp = SMCGetTemperature("TN1P");
            printf("    Temp TN1P        = %g\n", temp);
            temp = SMCGetTemperature("Th1H");
            printf("    Temp Th1H        = %g\n", temp);

        
        }

    return kIOReturnSuccess;
}

kern_return_t SMCPrintTemp(void)
{
    int i;
    for (i = 0; i < sizeof(temp_keys)/sizeof(temp_keys[0]); ++i) {
        double temp;
        temp = SMCGetTemperature(temp_keys[i].key);
        if (temp > 0)
            printf(" %20s : %g\n", temp_keys[i].name, temp);
    }
    return kIOReturnSuccess;
}

void usage(char* prog)
{
    printf("Apple System Management Control (SMC) tool %s\n", VERSION);
    printf("Usage:\n");
    printf("%s [options]\n", prog);
    printf("    -f         : fan info decoded\n");
    printf("    -t         : temperature info decoded\n");
    printf("    -h         : help\n");
    printf("    -k <key>   : key to manipulate\n");
    printf("    -l         : list all keys and values\n");
    printf("    -r         : read the value of a key\n");
    printf("    -w <value> : write the specified value to a key\n");
    printf("    -v         : version\n");
    printf("\n");
}


double SMCGetTemperature(char *key)
{
    SMCVal_t val;
    kern_return_t result;

    result = SMCReadKey(key, &val);
    if (result == kIOReturnSuccess) {
        // read succeeded - check returned value
        if (val.dataSize > 0) {
            if (strcmp(val.dataType, DATATYPE_SP78) == 0) {
                // convert fp78 value to temperature
                int intValue = (val.bytes[0] * 256 + val.bytes[1]) >> 2;
                return intValue / 64.0;
            }
        }
    }
    // read failed
    return 0.0;
}

kern_return_t SMCSetFanRpm(char *key, int rpm)
{
    SMCVal_t val;
    
    strcpy(val.key, key);
    val.bytes[0] = (rpm << 2) / 256;
    val.bytes[1] = (rpm << 2) % 256;
    val.dataSize = 2;
    return SMCWriteKey(val);
}

int SMCGetFanRpm(char *key)
{
    SMCVal_t val;
    kern_return_t result;

    result = SMCReadKey(key, &val);
    if (result == kIOReturnSuccess) {
        // read succeeded - check returned value
        if (val.dataSize > 0) {
            if (strcmp(val.dataType, DATATYPE_FPE2) == 0) {
                // convert FPE2 value to int value
                return (int)_strtof(val.bytes, val.dataSize, 2);
            }
        }
    }
    // read failed
    return -1;
}

int main(int argc, char *argv[])
{
    int c;
    extern char   *optarg;
    extern int    optind, optopt, opterr;

    kern_return_t result;
    int           op = OP_NONE;
    UInt32Char_t  key = "\0";
    SMCVal_t      val;

    while ((c = getopt(argc, argv, "fhk:lrw:vt")) != -1)
        {
            switch(c)
                {
                case 'f':
                    op = OP_READ_FAN;
                    break;
                case 'k':
                    if (strlen(optarg) > sizeof(UInt32Char_t)) {
                        memcpy(key, optarg, sizeof(UInt32Char_t));
                    } else {
                        memcpy(key, optarg, strlen(optarg));
                    }
                    break;
                case 'l':
                    op = OP_LIST;
                    break;
                case 'r':
                    op = OP_READ;
                    break;
                case 't':
                    op = OP_READ_TEMP;
                    break;
                case 'v':
                    printf("%s\n", VERSION);
                    return 0;
                    break;
                case 'w':
                    op = OP_WRITE;
                    {
                        int i;
                        char c[3];
                        for (i = 0; i < strlen(optarg); i++)
                            {
                                sprintf(c, "%c%c", optarg[i * 2], optarg[(i * 2) + 1]);
                                val.bytes[i] = (int) strtol(c, NULL, 16);
                            }
                        val.dataSize = i / 2;
                        if ((val.dataSize * 2) != strlen(optarg))
                            {
                                printf("Error: value is not valid\n");
                                return 1;
                            }
                    }
                    break;
                case 'h':
                case '?':
                    op = OP_NONE;
                    break;
                }
        }

    if (op == OP_NONE)
        {
            usage(argv[0]);
            return 1;
        }

    SMCOpen(&conn);

    switch(op)
        {
        case OP_LIST:
            result = SMCPrintAll();
            if (result != kIOReturnSuccess)
                printf("Error: SMCPrintAll() = %08x\n", result);
            break;
        case OP_READ:
            if (strlen(key) > 0)
                {
                    result = SMCReadKey(key, &val);
                    strncpy(val.key, key, 4);
                    if (result != kIOReturnSuccess)
                        printf("Error: SMCReadKey() = %08x\n", result);
                    else
                        printVal(val);
                }
            else
                {
                    printf("Error: specify a key to read\n");
                }
            break;
        case OP_READ_FAN:
            result = SMCPrintFans();
            if (result != kIOReturnSuccess)
                printf("Error: SMCPrintFans() = %08x\n", result);
            break;
        case OP_READ_TEMP:
            result = SMCPrintTemp();
            if (result != kIOReturnSuccess)
                printf("Error: SMCPrintFans() = %08x\n", result);
            break;
        case OP_WRITE:
            if (strlen(key) > 0)
                {
                    memcpy(val.key, key, strlen(key));
                    result = SMCWriteKey(val);
                    if (result != kIOReturnSuccess)
                        printf("Error: SMCWriteKey() = %08x\n", result);
                }
            else
                {
                    printf("Error: specify a key to write\n");
                }
            break;
        }

    SMCClose(conn);
    return 0;;
}
