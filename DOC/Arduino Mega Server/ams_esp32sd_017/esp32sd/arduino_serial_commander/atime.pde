/*
  Modul Time
  part of Arduino Serial Commander
  part of Arduino Mega Server project  
*/

long getTimeNow(){
  // java time is in ms, we want secs    
  Date d = new Date();
  Calendar cal = new GregorianCalendar();
  long current = d.getTime() / 1000;
  long timezone = cal.get(cal.ZONE_OFFSET) / 1000;
  long daylight = cal.get(cal.DST_OFFSET) / 1000;
  return current + timezone + daylight;
}

