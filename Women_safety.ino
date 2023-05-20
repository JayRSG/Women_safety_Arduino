// constants define
#define rightBtn 4
#define leftBtn 5
#define okBtn 2
#define buzzer 12
#define SP Serial
#define dl delay

// library header files
#include <SoftwareSerial.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>

// custmized function library
#include "Women_Safety.h"

// library based objects
LiquidCrystal_I2C lcd(0x27, 16, 2);
SoftwareSerial gsm(7, 8);

// global vars
int gsmState = -1;
int phoneState = -1;
bool gsmConnected = false;
bool backLightState = false;
short current_menu = -1;
bool triggerPanic = false;
short option = 0;
bool displayName = false;
bool timerStart = false;
int startTime = 0;
int endTime = 0;

struct Contacts
{
  String contact_name;
  String contact_number;
  Contacts *next;
  Contacts *previous;
};

struct Messages
{
  String message_body;
  Messages *next;
  Messages *previous;
};

Messages *messages_head = NULL;
Messages *current_message = messages_head;
Messages *latest_message = messages_head;

Contacts *head_contact = NULL;
Contacts *current_contact = head_contact;
Contacts *latest_contact = head_contact;

void addContacts(const char *name, const char *number)
{
  Contacts *contact = new Contacts();

  contact->contact_name = name;
  // strcpy(contact->contact_name, name);
  contact->contact_number = number;
  // strcpy(contact->contact_number, number);

  contact->next = NULL;
  contact->previous = NULL;

  if (head_contact == NULL)
  {
    head_contact = contact;
    current_contact = head_contact;
  }
  else
  {
    Contacts *current = head_contact;
    while (current->next != NULL)
    {
      current = current->next;
    }

    latest_contact = contact;
    contact->previous = current;
    current->next = contact;
  }
}

void addMessages(const char *body)
{
  Messages *message = new Messages();

  message->message_body = body;
  // strcpy(message->message_body, body);

  message->next = NULL;
  message->previous = NULL;

  if (messages_head == NULL)
  {
    messages_head = message;
    current_message = messages_head;
  }
  else
  {
    Messages *current = messages_head;
    while (current->next != NULL)
    {
      current = current->next;
    }

    latest_message = message;
    message->previous = current;
    current->next = message;
  }
}

// connects to gsm board
void initGSM()
{
  gsm.begin(9600);
  SP.println(F("GSM Init"));
  dl(1000);

  // Write AT init command
  gsm.println("AT");
  dl(1000);

  updateSerial("AT");

  gsmConnected = gsmState == ATOK ? true : false;

  if (gsmConnected)
  {
    backLight(true);
    SP.println(F("GSM Connected"));
    display_msg("GSM Connected", 1, 2000);
  }
  else
  {
    SP.println(F("GSM Connection Failed"));
    display_msg("GSM Connection Failed", 1, 2000);
  }
}

void updateSerial(String com)
{
  String response = "";

  while (gsm.available())
  {
    char c;
    c = gsm.read();
    response += c;

    if (c == '\n' && response.length() >= 2)
    {
      if (!beginsWith(response, "\r\n"))
      {
        if (contains(response, "AT+CPAS") || contains(response, "+CPAS:"))
        {
          phoneState = gsmResponse(com, response);
        }
        else if (!contains(response, "AT+CPAS") || !contains(response, "+CPAS:"))
        {
          gsmState = gsmResponse(com, response);
        }
      }
      parseGSMResponse();

      response = "";
    }
    else if (beginsWith(response, "\r\n"))
    {
      response = "";
    }
  }
}

int gsmResponse(String req, String res)
{
  SP.print("req: " + req);
  SP.println(" res: " + res);

  if (beginsWith(res, "RING"))
  {
    return ringing;
  }
  else if (beginsWith(res, "AT+CMGS="))
  {
    if (contains(req, "AT+CMGS"))
    {
      return smsSent;
    }
  }
  // Init code
  else if (beginsWith(res, "OK") && !contains(req, "AT+CPAS"))
  {
    if (contains(req, "ATD"))
    {
      return callInit;
    }
    else if (contains(req, "AT+CMGF=1"))
    {
      return smsTextMode;
    }
    else if (contains(req, "AT+CMGS"))
    {
      return smsSent;
    }
    else if (contains(req, "ATH"))
    {
      // Hang up
      return hangUpCall;
    }
    else if (contains(req, "AT"))
    {
      // OK
      return ATOK;
    }
  }
  // Errors
  else if (beginsWith(res, "ERROR") || beginsWith(res, "NO CARRIER") || beginsWith(res, "BUSY") || beginsWith(res, "NO ANSWER") || beginsWith(res, "NO DIALTONE") || beginsWith(res, "TIMEOUT") || beginsWith(res, "INVALID NUMBER") || beginsWith(res, "MEMORY FULL") || contains(res, "+CMS:"))
  {
    if (contains(req, "ATD") || contains(req, "AT+CPAS"))
    {
      return callFailed;
    }
    else if (contains(req, "AT+CMGS") || contains(req, "AT+CMGF=1"))
    {
      return smsFailed;
    }
  }
  else if (beginsWith(res, "UNKNOWN CALLING ERROR"))
  {
    if (contains(req, "ATD") || contains(req, "AT+CPAS"))
    {
      return unknown;
    }
  }
  // SMS
  else if (beginsWith(res, "+CMGF:") && (res.charAt(res.indexOf(':') + 2) - '0') == 1)
  {
    if (contains(req, "AT+CMGF?"))
    {
      return smsTextMode;
    }
  }
  // sms contact set, send body
  else if (contains(res, ">"))
  {
    if (contains(req, "AT+CMGS"))
    {
      return sendsmsBody;
    }
  }
  // Receive call
  else if (contains(res, "ATA"))
  {
    if (beginsWith(req, "ATA"))
    {
      return callInit;
    }
  }
  // Phone status
  else if (contains(req, "AT+CPAS"))
  {
    if (beginsWith(res, "+CPAS:"))
    {
      int state = (int)(res.charAt(res.indexOf(':') + 2) - '0');
      return phoneStatus(state);
    }
  }
  return -1;
}

int phoneStatus(int state)
{
  int ret = -1;
  SP.print(F("Phone Check: "));
  switch (state)
  {
  case 0:
    ret = ready;
    SP.println(F("OK"));
    break;
  case 1:
    ret = unavailable;
    SP.println(F("Unavailable"));
    break;
  case 2:
    ret = unknown;
    SP.println(F("Unknown"));
    break;
  case 3:
    ret = ringing;
    SP.println(F("Ringing"));
    break;
  case 4:
    ret = callInProgress;
    SP.println(F("Call In Progress"));
    break;
  case 5:
    ret = asleep;
    SP.println(F("Asleep"));
    break;
  default:
    ret = -1;
    SP.println(F("N/A"));
    break;
  }
  return ret;
}

void parseGSMResponse()
{
  if (gsmState != -1)
  {
    SP.print(F("GSM Status: "));
  }

  switch (gsmState)
  {
  case callInit:
    SP.println(F("Call Initialized"));
    break;
  case smsSent:
    SP.println(F("SMS Sent"));
    break;
  case callFailed:
    SP.println(F("Call Failed"));
    break;
  case smsFailed:
    SP.println(F("SMS Failed"));
    break;
  case ready:
    SP.println(F("GSM Ready"));
    break;
  case unknown:
    SP.println(F("Unknown State"));
    break;
  case callInProgress:
    SP.println(F("Call in Progress"));
    break;
  case ringing:
    SP.println(F("Incoming Call"));
    break;
  case asleep:
    SP.println(F("Device Asleep"));
    break;
  case alerting:
    SP.println(F("Device Alerting"));
    break;
  case incomingCall:
    SP.println(F("Incoming Call"));
    break;
  case hangUpCall:
    SP.println(F("Call Hung Up"));
    break;
  case ATOK:
    SP.println(F("OK"));
    break;
  case notallowed:
    SP.println(F("Operation not Allowed"));
    break;
  case smsTextMode:
    SP.println(F("Text Mode Enabled"));
    break;
  // case smsTracking:
  //   SP.println("SMS Tracking received");
  //   break;
  case unavailable:
    SP.println(F("Device Unavailable"));
    break;
  default:
    break;
  }
}

bool contains(String str, String data)
{
  if (str.indexOf(data) != -1)
  {
    return true;
  }
  else
  {
    return false;
  }
}

bool beginsWith(String str, String data)
{
  if (str.startsWith(data))
  {
    return true;
  }
  else
  {
    return false;
  }
}

bool readBtn(int btn)
{
  bool btnState = digitalRead(btn);
  dl(5);

  if (btnState)
  {
    return true;
  }
  else
  {
    return false;
  }

  return false;
}

void readSelectors()
{
  switch (current_menu)
  {
  case 0: // Display Menu
    if (readBtn(leftBtn) && !readBtn(okBtn))
    {
      option = abs(option - 1);
      option = option % 3;
    }
    else if (readBtn(rightBtn) && !readBtn(okBtn))
    {
      option = (++option) % 3;
    }
    else if (readBtn(okBtn))
    {
      if (option == 2)
      {
        triggerPanic = true;
        current_menu = 5;
      }
      else if (option == 0 || option == 1)
      {
        current_menu = 1;
        triggerPanic = false;
      }
    }
    break;
  case 1: // Contact Menu Number
  case 2: // Contact Menu Name
    if (readBtn(rightBtn) && !readBtn(okBtn))
    {
      current_contact = current_menu == 1 ? current_contact->next : current_contact;
      current_message = current_menu == 2 ? current_message->next : current_message;

      if (current_contact == NULL)
      {
        current_contact = head_contact;
      }

      if (current_message == NULL)
      {
        current_message = messages_head;
      }
    }
    else if (readBtn(leftBtn) && !readBtn(okBtn))
    {
      current_contact = current_menu == 1 ? current_contact->previous : current_contact;
      current_message = current_menu == 2 ? current_message->previous : current_message;

      if (current_contact == NULL)
      {
        current_contact = latest_contact;
      }

      if (current_message == NULL)
      {
        current_message = latest_message;
      }
    }
    else if (readBtn(okBtn) && (!readBtn(leftBtn) || !readBtn(rightBtn)))
    {
      if (current_menu == 1)
      {
        if (displayName)
        {
          if (option == 0)
          {
            current_menu = 2;
          }
          else
          {
            current_menu = 3;
          }
          displayName = false;
        }
        else
        {
          displayName = true;
          current_menu = 1;
        }
      }
      else if (current_menu == 2)
      {
        current_menu = 3;
        displayName = false;
      }
    }
    break;
  case 4: // Calling menu
    bool btnState = readBtn(okBtn);
    dl(20);
    if (phoneState == callInProgress || gsmState == callInit)
    {
      if (btnState || readBtn(okBtn))
      {
        hangCall();
      }
    }
    break;

  default:
    break;
  }
}

void displayMenu()
{
  lcd.clear();

  lcd.setCursor(5, 0);

  lcd.print("Menu");
  lcd.setCursor(0, 1);

  if (option == 0)
  {
    lcd.print("< sms > call sos");
  }
  else if (option == 1)
  {
    lcd.print("sms < call > sos");
  }
  else if (option == 2)
  {
    lcd.print("sms call < sos >");
  }
}

void displayContact()
{
  lcd.clear();

  lcd.setCursor(0, 0);

  lcd.print("Contacts: ");
  lcd.setCursor(0, 1);
  // int length = displayName ? strlen(current_contact->contact_name) : strlen(current_contact->contact_number);
  int length = displayName ? current_contact->contact_name.length() : current_contact->contact_number.length();

  if (current_contact != NULL)
  {
    displayName
        ? lcd.print(current_contact->contact_name)
        : lcd.print(current_contact->contact_number);
  }
}

void menuManagement()
{
  switch (current_menu)
  {
  case 0:
    displayMenu();
    break;
  case 1:
    displayContact();
    break;
  case 2:
    if (option == 0)
    {
      display_msg(current_message->message_body, 0);
    }
    break;
  case 3:
    communication_action();
    break;
  case 4:
    checkGSMStatus();

    if (gsmState == callFailed || phoneState == ready)
    {
      current_menu = 0;
    }
    break;
  case 5:
    // panicMode(triggerPanic);
    break;
  default:
    break;
  }

  readSelectors();

  if (!gsmConnected)
  {
    initGSM();
    dl(3000);
  }
}

void PanicBtnPress(bool off = false)
{
  // panic mode on
  if (!off)
  {
    if (current_menu == -1 && !timerStart && readBtn(leftBtn))
    {
      timerStart = true;
      startTime = millis();
    }

    if (current_menu == -1 && timerStart && readBtn(leftBtn))
    {
      endTime = millis();
      timerStart = false;
      triggerPanic = true;
      current_contact = head_contact;
    }
  }
  // panic mode off
  else if (off)
  {
    // Stop Panic Mode Init
    if (current_menu == 5 && !timerStart && readBtn(leftBtn))
    {
      timerStart = true;
      startTime = millis();
    }

    // Stop Panic Mode Second Press
    if (current_menu == 5 && timerStart && readBtn(leftBtn))
    {
      endTime = millis();
      timerStart = false;
    }
  }
}

void NormalBtnPress()
{
  // Start Normal Mode on a single ok btn press
  if ((current_menu == -1) && readBtn(okBtn))
  {
    current_menu = 0;
    displayName = false;
    backLight(true);
  }
}

void resolveBtnPress()
{
  // start / end panic mode
  if (endTime && startTime && !timerStart && (endTime - startTime) >= 100 && (endTime - startTime) < 1000)
  {
    if (current_menu == 5 && triggerPanic)
    {
      // display_msg("Welcome", 3);
      backLight(false);
      current_menu = -1;
      triggerPanic = false;
    }
  }
  else
  {
    startTime = 0;
    endTime = 0;
  }

  if (triggerPanic)
  {
    backLight(true);
    current_menu = 5;
  }
}

void initBtnPress()
{
  PanicBtnPress();
  PanicBtnPress(true);
  NormalBtnPress();

  resolveBtnPress();
}

void makeCall()
{
  // gsm.println(strcat(strcat("ATD", current_contact->contact_number), ";\r"));
  gsm.println("ATD" + current_contact->contact_number + ";");

  dl(1000);
  updateSerial("ATD");
}

void hangCall()
{
  gsm.println("ATH");
  dl(1000);
  updateSerial("ATH");

  if (gsmState == hangUpCall)
  {
    display_msg("Call Hung Up", 1, 1000);
  }
}

void checkGSMStatus()
{
  gsm.println("AT+CPAS");
  dl(1000);
  updateSerial("AT+CPAS");
}

void sms(String sms_body = "")
{
  display_msg("Sending SMS TO:");
  SP.println(F("Sending SMS to"));
  display_msg(current_contact->contact_number);
  SP.println(current_contact->contact_number);

  // CONFIG text mode
  gsm.println("AT+CMGF=1\r\n");
  dl(1000);
  updateSerial("AT+CMGF=1");

  if (gsmState == smsTextMode && gsmState != smsFailed)
  {
    // set receipient #
    gsm.print("AT+CMGS=\"");
    gsm.print(current_contact->contact_number);
    gsm.println("\"\r");
    // dl(2000);
    // updateSerial("AT+CMGS");

    dl(1000);

    sms_body == "" ? gsm.print(current_message->message_body) : gsm.print(sms_body);
    gsm.write(26);
    dl(2000);
    updateSerial("AT+CMGS");

    if (gsmState == smsSent)
    {
      display_msg("SMS Sent", 1, 2000);
      SP.println(F("SMS Sent"));
      if (current_menu != 5)
      {
        current_menu = 0;
      }
    }
    else if (gsmState == smsFailed)
    {
      if (current_menu != 5)
      {
        current_menu = 0;
      }
      display_msg("SMS Failed", 1, 2000);
      SP.println(F("SMS Sending Failed"));
    }
  }
  else
  {
    display_msg("SMS Sending Failed", 1, 2000);
    SP.println(F("SMS Sending Failed"));
    if (current_menu != 5)
    {
      current_menu = 0;
    }
  }
}

void call()
{
  makeCall();
  dl(2000);

  backLight(true);
  if (gsmState == callFailed)
  {
    current_menu = -1;

    display_msg("Call Failed", 1);
  }

  if (gsmState == callInit && current_menu != 5)
  {
    current_menu = 4;
    SP.print(F("Calling "));
    SP.println(current_contact->contact_name);

    display_msg("Calling " + String(current_contact->contact_name), 1);
  }
}

void communication_action()
{
  lcd.clear();
  lcd.home();
  if (gsmConnected)
  {
    switch (option)
    {
    case 0:
      sms();
      break;
    case 1:
      call();
      break;
    default:
      break;
    }
  }
  else
  {
    current_menu = -1;
    SP.println(F("GSM Connection Failed"));
    display_msg("GSM Connection Failed", 1, 1000);
  }
}

void backLight(bool mode)
{
  mode ? lcd.backlight() : lcd.noBacklight();
  backLightState = mode;
}

void display_msg(String msg, int type = 1, int del = 0)
{
  lcd.clear();
  lcd.setCursor(0, 0);

  switch (type)
  {
  case 0:
    lcd.print(F("SMS:      "));
    break;
  case 1:
    lcd.print(F("Network:    "));
    break;
  case 2:
    lcd.print(F("Contact:     "));
    break;
  case 3:
    lcd.print(F("We Safety"));
    break;
  }
  lcd.setCursor(0, 1);
  lcd.print(msg.substring(0, 16));

  // Scroll the message j times
  if (msg.length() > 16)
  {
    lcd.setCursor(0, 1);
    lcd.print(msg.substring(0, 16));
    for (int i = 0; i < msg.length() - 16; i++)
    {
      dl(500); // Delay between scrolling steps

      // Shift the message on the second row by one character to the left
      lcd.setCursor(0, 1);
      lcd.print(msg.substring(i + 1, i + 17));
    }
    dl(1000);
    readSelectors();
    backNav();
  }
  dl(del);
}

void beep(int highTime, int freq = 1, int lowTime = 0)
{
  if (lowTime == 0)
  {
    lowTime = highTime;
  }

  while (freq--)
  {
    digitalWrite(buzzer, HIGH);
    dl(highTime);
    digitalWrite(buzzer, LOW);
    dl(lowTime);
  }
}

void backNav()
{
  if (readBtn(rightBtn) && readBtn(leftBtn) && !readBtn(okBtn))
  {
    current_menu--;
    current_menu = current_menu < 0 || current_menu == 5 ? 0 : current_menu;
  }
}

// setup function which runs only once
void setup()
{
  // total 3 input buttons, 2 next and previous selector, 1 menu selector
  pinMode(rightBtn, INPUT);
  pinMode(leftBtn, INPUT);
  pinMode(okBtn, INPUT);
  pinMode(buzzer, OUTPUT);

  /** initialize data communication for Serial and LCD*/
  SP.begin(9600);
  lcd.init();
  lcd.home();
  backLight(true);
  Wire.begin();

  // Welcome Message
  display_msg("Welcome", 3, 1000);

  // Initialize GSM
  gsmState = -1;
  initGSM();
  /**Adding Contacts*/
  addContacts("Joy Ram Sen Gupta", "+8801760060004");
  addContacts("Falguni Sengupta", "+8801760060005");

  addMessages("I am in distress");
  addMessages("Save my Soul");
  addMessages("I am safe");
  addMessages("Heading home");

  backLight(false);

  checkGSMStatus();
}

// the main loop, runs continuously as long as the board is on
void loop()
{

  backNav();
  initBtnPress();
  menuManagement();

  readSelectors();
  dl(200);
}