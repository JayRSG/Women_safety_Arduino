#ifndef WOMEN_SAFETY_H
#define WOMEN_SAFETY_H

#define noState -1
#define callInit 0
#define smsSent 1
#define callFailed 2
#define smsFailed 3
#define ready 4
#define unknown 5
#define callInProgress 6
#define ringing 7
#define asleep 8
#define alerting 9
#define incomingCall 10
#define hangUpCall 11
#define ATOK 12
#define notallowed 13
#define smsTextMode 14
#define unavailable 15
#define sendsmsBody 16
// #define smsTracking 17

// Function forward decl
void initGSM();
int gsmResponse(String req, String res);
void checkGSMStatus();
bool readBtn(int btn);
void parseGSMResponse();
int phoneStatus(int state);
bool beginsWith(String str, String data);
bool contains(String str, String data);
void updateSerial(String com);
void display_msg(String msg, int type = 1, int del = 0);
void backLight(bool mode);
void beep(int highTime, int freq = 1, int lowTime = 0);
void readSelectors();
void displayMenu();
void displayContact();
void panicBtnPress();
void normalBtnPress();
void resolveBtnPress();
void initBtnPress();
void backNav();
void communication_action();
void sms(String sms_body = "");
void call();
void makeCall();
void menuManagement();
void hangCall();
void panicMode();

#endif