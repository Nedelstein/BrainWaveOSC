#include "ofxThinkgear.h"


// check code definitions in ThinkGearStreamParser.h
void tgHandleDataValueFunc( unsigned char extendedCodeLevel, unsigned char code, unsigned char valueLength, const unsigned char *value, void *customData){
    ofxThinkgear& tg = *reinterpret_cast<ofxThinkgear*>(customData);
    if (extendedCodeLevel == 0){
        switch (code) {
            case PARSER_CODE_BATTERY:
                tg.values.battery = value[0] & 0xff;
                ofNotifyEvent(tg.onBattery, tg.values);
                break;
            case PARSER_CODE_POOR_QUALITY:
                tg.values.poorSignal = value[0] & 0xff;
                ofNotifyEvent(tg.onPoorSignal, tg.values);
                break;
            case PARSER_CODE_ATTENTION:
                tg.values.attention = value[0] & 0xff;
                ofNotifyEvent(tg.onAttention, tg.values);
                break;
            case PARSER_CODE_MEDITATION:
                tg.values.meditation = value[0] & 0xff;
                ofNotifyEvent(tg.onMeditation, tg.values);
                break;
            case 0x16:
                tg.values.blinkStrength = value[0] & 0xff;
                ofNotifyEvent(tg.onBlinkStrength, tg.values);
            case( 0xd4 ):
                // printf("Standby... autoconnecting\n");
                if(tg.allowRawDataEvents) ofNotifyEvent(tg.onConnecting, tg.values);
                tg.device->writeByte(0xc2); // what is this?
                break;
            case( 0xd0 ):
                ofNotifyEvent(tg.onReady, tg.values);
                break;
            case( 0xd1 ):
                {
                    ofMessage err("Headset not found");
                    ofNotifyEvent(tg.onError, err);
                }
                break;
            case PARSER_CODE_RAW_SIGNAL:
                tg.values.raw = (value[0] << 8) | value[1];
                ofNotifyEvent(tg.onRaw, tg.values);
                break;
            case PARSER_CODE_ASIC_EEG_POWER_INT:
                {
                    //131?
                    //eegPower[j] = ((unsigned long)packetData[++i] << 16) | ((unsigned long)packetData[++i] << 8) | (unsigned long)packetData[++i];
                    int pos = 0;
                    tg.values.eegDelta = (value[pos] << 16) | (value[pos+1] << 8) | (value[pos+2]); pos += 3;
                    tg.values.eegTheta = (value[pos] << 16) | (value[pos+1] << 8) | (value[pos+2]); pos += 3;
                    tg.values.eegLowAlpha = (value[pos] << 16) | (value[pos+1] << 8) | (value[pos+2]); pos += 3;
                    tg.values.eegHighAlpha = (value[pos] << 16) | (value[pos+1] << 8) | (value[pos+2]); pos += 3;
                    tg.values.eegLowBeta = (value[pos] << 16) | (value[pos+1] << 8) | (value[pos+2]); pos += 3;
                    tg.values.eegHighBeta = (value[pos] << 16) | (value[pos+1] << 8) | (value[pos+2]); pos += 3;
                    tg.values.eegLowGamma = (value[pos] << 16) | (value[pos+1] << 8) | (value[pos+2]); pos += 3;
                    tg.values.eegMidGamma = (value[pos] << 16) | (value[pos+1] << 8) | (value[pos+2]); pos += 3;
                    ofNotifyEvent(tg.onEeg, tg.values);
                    break;
                }
            /* Other [CODE]s */
            default:
                printf( "EXCODE level: %d CODE: 0x%02X vLength: %d\n", extendedCodeLevel, code, valueLength );
                printf( "Data value(s):" );
                for( int i=0; i<valueLength; i++ ) printf( " %02X", value[i] & 0xFF );
                printf( "\n" );
                break;
        }
    }
}


ofxThinkgear::ofxThinkgear() : isReady(false) {
    
    device = new ofSerial();
    // print all devices to console
    device->listDevices();
    allowRawDataEvents = false;
    parserSetup = false;
    attempts = 0;
    unavailableCount = 0;
    noConnectionRestartCount = 250;
    noDataRestartCount = 500;
}

ofxThinkgear::~ofxThinkgear(){
    close();
}

void ofxThinkgear::setup(string deviceName, int baudRate) {
    this->deviceName = deviceName;
    this->baudRate = baudRate;
}

void ofxThinkgear::close(){
    if (isReady){
        device->writeByte(0xC1);
        device->flush();
        //device.drain();
        device->close();
        isReady = false;
    }
}

//int unavailableCount = 0;

void ofxThinkgear::idle() {
    if (isReady) {
        int n = device->available();
        if (n > 0){
            //unavailableCount = 0;
            n = device->readBytes(buffer, min(n,512));
        }
    }
}

void ofxThinkgear::update(){
    if (!isReady && ofGetFrameNum() % noConnectionRestartCount == 0){
        ofLog() << "connecting to device...";
        attempts++;
        if (device->setup(deviceName, baudRate)){
            device->flush();
            //if(!parserSetup) {
                int parserInited = THINKGEAR_initParser(&parser, PARSER_TYPE_PACKETS, tgHandleDataValueFunc, this);
                //parserSetup = true;
                ofLog() << "parser setup: " << parserInited;;
            //}
            isReady = true;
            ofLog() << "ThinkGear device setup";
            attempts = 0;
        }
        
    }
    if (!isReady)
        return;
    int n = device->available();
    if (n > 0){
        unavailableCount = 0;
        n = device->readBytes(buffer, min(n,512));
        for (int i=0; i<n; ++i){
            THINKGEAR_parseByte(&parser, buffer[i]);
        }
    } else {
        
        // reconnection problems fix (connected but no data). made device a pointer that gets deleted and reassigned
        // test by switching device off/on
        //ofLog() << "no data...";
        unavailableCount++;
        if(unavailableCount >  noDataRestartCount) {
            ofLog() << "*** no data available - attempt to reconnect";
            isReady = false;
            attempts = 0;
            unavailableCount = 0;
            device->close();
            delete device;
            device = NULL;
            
            ofSerial* retryDevice = new ofSerial();
            device = retryDevice;
        }
    }
}

void ofxThinkgear::flush(){
    if (isReady)
        device->flush();
}

