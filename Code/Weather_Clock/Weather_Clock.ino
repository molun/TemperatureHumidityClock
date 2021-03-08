#include <ArduinoJson.h>         
#include <DNSServer.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266_Seniverse.h>
#include <NTPClient.h>
#include <U8g2lib.h>
#include <WiFiUdp.h>
#include <WiFiManager.h> 
#include <Wire.h>
#include "MyFont.h"

String reqUserKey = "Sx1iKh*****D6SoDi";                // 心知天气密钥
String reqLocation = "taiyuan";                         // 城市拼音
String reqUnit = "c";                                   // 设置单位为 ℃

//0为今天的数据，1为明天的数据，2为后天的数据
//         天气代码                最高气温                      最低气温                    湿度
String Weather_Code[3] , Weather_HighTemperature[3] , Weather_LowTemperature[3] , Weather_Humidity[3];

//闰年平年的天数
const unsigned short int mon_yday[][13] =
{
    { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365 },    //平年
    { 0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335, 366 }     //闰年
};

int Nowyear , Nowmonth , Nowday , Nowweek , Nowhour , Nowminute , Nowsecond ;
int count=0;      //用于控制显示页面：0显示时间，1显示今天天气，2显示明天天气，3显示后天天气

WiFiUDP ntpUDP;            // 建立NTP对象
Forecast forecast;         // 建立Forecast对象用于获取心知天气3天预报信息
WiFiManager wifiManager;   // 建立WiFiManager对象用于配置Wifi

NTPClient timeClient( ntpUDP , "ntp1.aliyun.com" , 60*60*8 , 30*60*1000 );    //初始化NTP连接，NTP服务器为阿里云服务器，设置时区为东8区，北京

//构造oled初始化函数
//0.96寸OLED 驱动SSD1306
U8G2_SSD1306_128X64_NONAME_1_SW_I2C u8g2(U8G2_R0, /* clock=*/ SCL, /* data=*/ SDA, /* reset=*/ U8X8_PIN_NONE);

//1.3寸OLED 驱动SH1106
//U8G2_SH1106_128X64_NONAME_1_SW_I2C u8g2(U8G2_R0, /* clock=*/ SCL, /* data=*/ SDA, /* reset=*/ U8X8_PIN_NONE);

void setup()
{
  u8g2.begin();       //u8g2初始化，用于oled显示
  attachInterrupt(D4,Key_Detectuin,FALLING);      //外部中断引脚设置为D4，下降沿触发，中断服务函数为 Key_Detectuin
  Oled_Display_Start();       //oled显示开始界面
  wifiManager.autoConnect("Weather_Clock", "tymishop");     //创建Wifi用于配置网络，Wifi名 Weather_Clock ，密码 tymishop
  Oled_Display_Success();       //oled显示Wifi连接成功，连接的Wifi名和IP地址
  timeClient.begin();       //NTP连接初始化
  forecast.config(reqUserKey, reqLocation, reqUnit);      //心知天气对象初始化
  Get_WeatherForecast();        //获取天气
  timeClient.update();          //更新一次NTP时间
  GetDateAndTime(timeClient.getEpochTime());        //计算年月日时分秒
  delay(2000);          //延时2s
}

void loop() 
{
  timeClient.update();          //更新一次NTP时间
  GetDateAndTime(timeClient.getEpochTime());        //计算年月日时分秒
  if((Nowminute%15)==0&&Nowsecond%60==0)           //在xx:00:00 xx:15:00 xx:30:00 xx:45:00时更新天气
  {
    Get_WeatherForecast();          //获取天气
  }
  if(count==0)          //如果处于时间显示界面，则显示时间
  {
    Oled_Display_Time();      //oled显示时间
  }
  if(WiFi.status()!= WL_CONNECTED)      //如果检测到Wifi断开，重新配置网络
  {
    Oled_Display_Fail();        //oled显示Wifi断开请重连
    wifiManager.autoConnect("Weather_Clock", "tymishop");   //创建Wifi用于配置网络，Wifi名 Weather_Clock ，密码 tymishop
    Oled_Display_Success();     //oled显示Wifi连接成功，连接的Wifi名和IP地址
    delay(3000);            //延时3s
  }
  delayMicroseconds(804582);    //一次循环 程序运行总耗时195418us，为了1s刷新一次屏幕，延迟 1000000-195418=804582us   
}

//判断一个年份是否为闰年，是就返回1，不是就返回0
inline int isLeapYear(int years)
{
    return( (years%4 == 0 && years%100 != 0) || (years%400 == 0) );
}

//获取一年的天数，闰年返回366，平年返回355
inline int getDaysForYear(int years)
{
    if(isLeapYear(years))
    {
      return 366;
    }
    else
    {
      return 365;
    }
}

//根据秒数计算日期和时间
void GetDateAndTime(int seconds)
{
    int days = seconds / SECOND_DAY;
    int curYear = START_YEAR;
    int leftDays = days;
    int leftSeconds = seconds % SECOND_DAY;

    //计算星期几和时间
    Nowweek = (days+START_WEEK)%7;  
    Nowhour = leftSeconds / SECOND_HOUR;
    Nowminute = (leftSeconds % SECOND_HOUR) / SECOND_MIN;
    Nowsecond = leftSeconds % SECOND_MIN;
    
    //计算年份
    int daysCurYear = getDaysForYear(curYear);
    while (leftDays >= daysCurYear)
    {
        leftDays -= daysCurYear;
        curYear++;
        daysCurYear = getDaysForYear(curYear);
    }
    Nowyear = curYear;

    //计算日期
    int isLeepYear = isLeapYear(curYear);
    for (int i = 1; i < 13; i++)
    {
        if (leftDays < mon_yday[isLeepYear][i])
        {
            Nowmonth = i;
            Nowday = leftDays - mon_yday[isLeepYear][i-1] + 1;
            break;
        }
    }
}

//获取今天、明天、后天的天气情况
void Get_WeatherForecast(void)
{
  int j;
  if(forecast.update())       // 更新天气信息
  {
    for(j=0;j<=2;j++)
    {
      Weather_Code[j]=forecast.getDayCode(j);
      Weather_HighTemperature[j]=forecast.getHigh(j);
      Weather_LowTemperature[j]=forecast.getLow(j);
      Weather_Humidity[j]=forecast.getHumidity(j);
    }  
  } 
}

//oled显示时间
void Oled_Display_Time( void )
{
  u8g2.firstPage();                              
  do {
       u8g2.drawXBM( 76, 1 , 36 , 18, u8g_hzxq_bits);   //显示汉字星期
       switch(Nowweek)        //对应星期几就显示几，其中0为星期日
       {
          case 0: u8g2.drawXBM( 112, 2 , 18 , 18, u8g_hzszri_bits);
                  break;
          case 1: u8g2.drawXBM( 112, 2 , 18 , 18, u8g_hzszyi_bits);
                  break;
          case 2: u8g2.drawXBM( 112, 2 , 18 , 18, u8g_hzszer_bits);
                  break;
          case 3: u8g2.drawXBM( 112, 2 , 18 , 18, u8g_hzszsan_bits);
                  break;
          case 4: u8g2.drawXBM( 112, 2 , 18 , 18, u8g_hzszsi_bits);
                  break;
          case 5: u8g2.drawXBM( 112, 2 , 18 , 18, u8g_hzszwu_bits);
                  break;
          case 6: u8g2.drawXBM( 112, 2 , 18 , 18, u8g_hzszliu_bits);
                  break;
       }
       u8g2.setFont(u8g2_font_ncenB12_tr);
       u8g2.drawStr(4,13,"        -    -    "); 
       u8g2.setFont(u8g2_font_ncenB10_tr);
       u8g2.setCursor(4,15);
       u8g2.print(Nowyear); //显示年
       if(Nowmonth>9)       //根据月份是几位数调整显示位置
       {
         u8g2.setCursor(41,15);
       }
       else
       {
         u8g2.setCursor(45,15);
       }
       u8g2.print(Nowmonth);    //显示月份
       if(Nowday>9)         //根据日期是几位数调整显示位置
       {
         u8g2.setCursor(62,15);
       }
       else
       {        
         u8g2.setCursor(67,15);
       }
       u8g2.print(Nowday);    //显示是几号
       u8g2.setFont(u8g2_font_ncenB18_tr);      
       u8g2.drawStr(44,46,":");
       u8g2.drawStr(90,46,":");
       u8g2.setFont(u8g2_font_ncenB24_tr);
       u8g2.setCursor(5,52);
       u8g2.print(Nowhour/10);    //显示小时十位数
       u8g2.setCursor(25,52);     
       u8g2.print(Nowhour%10);    //显示小时个位数
       u8g2.setCursor(51,52);
       u8g2.print(Nowminute/10);  //显示分钟十位数
       u8g2.setCursor(71,52);
       u8g2.print(Nowminute%10);  //显示分钟个位数
       u8g2.setFont(u8g2_font_ncenB14_tr);
       u8g2.setCursor(98,50);
       u8g2.print(Nowsecond/10);  //显示秒十位数
       u8g2.setCursor(109,50);
       u8g2.print(Nowsecond%10);  //显示秒个位数
     } while ( u8g2.nextPage() );
}

//oled显示天气
void Oled_Display_Weather( int i )
{
  u8g2.clearDisplay();
  do {  
        u8g2.drawXBM( 52 , 0  , 32 , 20 , u8g_hzcity_bits);     //显示汉字城市名
        switch(i)   //根据i显示汉字今天或明天或后天
        {
          case 1: u8g2.drawXBM( 90 , 0  , 32 , 20 , u8g_hzjt_bits);  //显示汉字今天
                  break;
          case 2: u8g2.drawXBM( 90 , 0  , 32 , 20 , u8g_hzmt_bits);  //显示汉字明天
                  break;
          case 3: u8g2.drawXBM( 90 , 0  , 32 , 20 , u8g_hzht_bits);  //显示汉字后天
                  break;
        }
        u8g2.drawXBM( 108  , 23  , 20 , 20 , u8g_sheshidu_bits);    //显示 ℃
        u8g2.drawXBM( 50  , 44  , 32 , 20 , u8g_hzsd_bits);         //显示汉字湿度
        u8g2.setFont(u8g2_font_ncenB12_tr);
        u8g2.drawStr( 75, 37, "~");
        u8g2.setCursor(50, 39);
        u8g2.print(Weather_LowTemperature[i-1]);        //显示最低气温
        u8g2.setCursor(86, 39);
        u8g2.print(Weather_HighTemperature[i-1]);       //显示最高气温
        u8g2.drawStr( 83, 60, ":");
        u8g2.setCursor(92, 60);
        u8g2.print(Weather_Humidity[i-1]);              //显示湿度
        //根据天气代码显示对应的天气图标和天气名称
        if(Weather_Code[i-1]=="0"||Weather_Code[i-1]=="1"||Weather_Code[i-1]=="2"||Weather_Code[i-1]=="3")      //晴
        {
          u8g2.drawXBM( 5  , 2  , 40 , 40 , u8g_qing_bits);
          u8g2.drawXBM( 16 , 43 , 18 , 20 , u8g_hzqing_bits);
        }
        else if(Weather_Code[i-1]=="4"||Weather_Code[i-1]=="5"||Weather_Code[i-1]=="6"||Weather_Code[i-1]=="7"||Weather_Code[i-1]=="8")      //多云
        {
          u8g2.drawXBM( 5 , 2  , 40 , 40 , u8g_duoyun_bits);
          u8g2.drawXBM( 9 , 43 , 32 , 20 , u8g_hzduoyun_bits);
        }
        else if(Weather_Code[i-1]=="9")      //阴
        {
          u8g2.drawXBM( 5  , 2  , 40 , 40 , u8g_yin_bits);
          u8g2.drawXBM( 16 , 43 , 18 , 20 , u8g_hzyin_bits);
        }
        else if(Weather_Code[i-1]=="10")      //阵雨
        {
          u8g2.drawXBM( 5 , 2  , 40 , 40 , u8g_zhenyu_bits);
          u8g2.drawXBM( 9 , 43 , 32 , 20 , u8g_hzzhenyu_bits);
        }
        else if(Weather_Code[i-1]=="11")      //雷阵雨
        {
          u8g2.drawXBM( 5 , 2  , 40 , 40 , u8g_leizhenyu_bits);
          u8g2.drawXBM( 2 , 43 , 45 , 20 , u8g_hzleizhenyu_bits);
        }
        else if(Weather_Code[i-1]=="12")      //雷阵雨伴有冰雹
        {
          u8g2.drawXBM( 5 , 2  , 40 , 40 , u8g_leibingyu_bits);
          u8g2.drawXBM( 2 , 43 , 45 , 20 , u8g_hzleibingyu_bits);
        }
        else if(Weather_Code[i-1]=="13")      //小雨
        {
          u8g2.drawXBM( 5 , 2  , 40 , 40 , u8g_xiaoyu_bits);
          u8g2.drawXBM( 9 , 43 , 32 , 20 , u8g_hzxiaoyu_bits);
        }
        else if(Weather_Code[i-1]=="14")      //中雨
        {
          u8g2.drawXBM( 5 , 2  , 40 , 40 , u8g_zhongyu_bits);
          u8g2.drawXBM( 9 , 43 , 32 , 20 , u8g_hzzhongyu_bits);
        }
        else if(Weather_Code[i-1]=="15")      //大雨
        {
          u8g2.drawXBM( 5 , 2  , 40 , 40 , u8g_dayu_bits);
          u8g2.drawXBM( 9 , 43 , 32 , 20 , u8g_hzdayu_bits);
        }
        else if(Weather_Code[i-1]=="16")      //暴雨
        {
          u8g2.drawXBM( 5 , 2  , 40 , 40 , u8g_baoyu_bits);
          u8g2.drawXBM( 9 , 43 , 32 , 20 , u8g_hzbaoyu_bits);
        }
        else if(Weather_Code[i-1]=="17")      //大暴雨
        {
          u8g2.drawXBM( 5 , 2  , 40 , 40 , u8g_dabaoyu_bits);
          u8g2.drawXBM( 2 , 43 , 45 , 20 , u8g_hzdabaoyu_bits);
        }
        else if(Weather_Code[i-1]=="18")      //特大暴雨
        {
          u8g2.drawXBM( 5 , 2  , 40 , 40 , u8g_jubaoyu_bits);
          u8g2.drawXBM( 2 , 43 , 45 , 20 , u8g_hzjubaoyu_bits);
        }
        else if(Weather_Code[i-1]=="19")      //冻雨
        {
          u8g2.drawXBM( 5 , 2  , 40 , 40 , u8g_dongyu_bits);
          u8g2.drawXBM( 9 , 43 , 32 , 20 , u8g_hzdongyu_bits);
        }
        else if(Weather_Code[i-1]=="20")      //雨夹雪
        {
          u8g2.drawXBM( 5 , 2  , 40 , 40 , u8g_yujiaxue_bits);
          u8g2.drawXBM( 2 , 43 , 45 , 20 , u8g_hzyujiaxue_bits);
        }
        else if(Weather_Code[i-1]=="21")      //阵雪
        {
          u8g2.drawXBM( 5 , 2  , 40 , 40 , u8g_zhenxue_bits);
          u8g2.drawXBM( 9 , 43 , 32 , 20 , u8g_hzzhenxue_bits);
        }
        else if(Weather_Code[i-1]=="22")      //小雪
        {
          u8g2.drawXBM( 5 , 2  , 40 , 40 , u8g_xiaoxue_bits);
          u8g2.drawXBM( 9 , 43 , 32 , 20 , u8g_hzxiaoxue_bits);
        }
        else if(Weather_Code[i-1]=="23")      //中雪
        {
          u8g2.drawXBM( 5 , 2  , 40 , 40 , u8g_zhongxue_bits);
          u8g2.drawXBM( 9 , 43 , 32 , 20 , u8g_hzzhongxue_bits);
        }
        else if(Weather_Code[i-1]=="24")      //大雪
        {
          u8g2.drawXBM( 5 , 2  , 40 , 40 , u8g_daxue_bits);
          u8g2.drawXBM( 9 , 43 , 32 , 20 , u8g_hzdaxue_bits);
        }
        else if(Weather_Code[i-1]=="25")      //暴雪
        {
          u8g2.drawXBM( 5 , 2  , 40 , 40 , u8g_baoxue_bits);
          u8g2.drawXBM( 9 , 43 , 32 , 20 , u8g_hzbaoxue_bits);
        }
        else if(Weather_Code[i-1]=="26")      //浮尘
        {
          u8g2.drawXBM( 5 , 2  , 40 , 40 , u8g_fuchen_bits);
          u8g2.drawXBM( 9 , 43 , 32 , 20 , u8g_hzfuchen_bits);
        }
        else if(Weather_Code[i-1]=="27")      //扬沙
        {
          u8g2.drawXBM( 5 , 2  , 40 , 40 , u8g_yangsha_bits);
          u8g2.drawXBM( 9 , 43 , 32 , 20 , u8g_hzyangsha_bits);
        }
        else if(Weather_Code[i-1]=="28"||Weather_Code[i-1]=="29")      //沙尘暴
        {
          u8g2.drawXBM( 5 , 2  , 40 , 40 , u8g_shachenbao_bits);
          u8g2.drawXBM( 2 , 43 , 45 , 20 , u8g_hzshachenbao_bits);
        }
        else if(Weather_Code[i-1]=="30")      //雾
        {
          u8g2.drawXBM( 5  , 2  , 40 , 40 , u8g_wu_bits);
          u8g2.drawXBM( 16 , 43 , 18 , 20 , u8g_hzwu_bits);
        }
        else if(Weather_Code[i-1]=="31")      //霾
        {
          u8g2.drawXBM( 5  , 2  , 40 , 40 , u8g_mai_bits);
          u8g2.drawXBM( 16 , 43 , 18 , 20 , u8g_hzmai_bits);
        }
  } while ( u8g2.nextPage() ); 
}

//D4引脚外部中断函数
ICACHE_RAM_ATTR void Key_Detectuin()
{
  while((digitalRead(D4))==0);    //等待松开按键
  count++;            //用于控制显示页面：0显示时间，1显示今天天气，2显示明天天气，3显示后天天气
  if(count==4)
  {
    count=0;  
  }
  switch(count)
  {
    case 0: Oled_Display_Time();
            break;
    case 1: Oled_Display_Weather(1);
            break;
    case 2: Oled_Display_Weather(2);
            break;
    case 3: Oled_Display_Weather(3);
            break;
  } 
}

void Oled_Display_Start(void)
{
  u8g2.firstPage();                              
  do {
       u8g2.drawXBM( 0, 2 , 128 , 18, u8g_hzqljWifipzwl_bits);    //显示 请连接Wifi配置网络
       u8g2.drawXBM( 0, 22 , 128 , 18, u8g_WeatherClock_bits);    //显示 Wifi：WeatherClock
       u8g2.drawXBM( 0, 42 , 128 , 18, u8g_mmtymishop_bits);      //显示 密码：tymishop
     } while ( u8g2.nextPage() );
}

void Oled_Display_Success(void)
{
  u8g2.firstPage();                              
  do {
       u8g2.drawXBM( 9, 2 , 110 , 20, u8g_Wifiljcg_bits);   //显示 Wifi连接成功
       u8g2.drawXBM( 0, 25 , 36 , 16, u8g_Wifimh_bits);     //显示 Wifi：
       u8g2.drawXBM( 0, 47 , 18 , 16, u8g_IPmh_bits);       //显示 IP：
       u8g2.setFont(u8g2_font_ncenB12_tr);
       u8g2.setCursor(40,37);
       u8g2.print(WiFi.SSID());                             //显示连接的Wifi名
       u8g2.setFont(u8g2_font_ncenB10_tr);
       u8g2.setCursor(19,61);
       u8g2.print(WiFi.localIP());                          //显示连接的IP地址
     } while ( u8g2.nextPage() );
}

void Oled_Display_Fail(void)
{
  u8g2.firstPage();                              
  do {  
       u8g2.drawXBM( 4, 2 , 120 , 20, u8g_Wifiydkqcl_bits);     //显示 Wifi已断开请重连
       u8g2.drawXBM( 0, 24 , 128 , 18, u8g_WeatherClock_bits);  //显示 Wifi：WeatherClock
       u8g2.drawXBM( 0, 44 , 128 , 18, u8g_mmtymishop_bits);    //显示 密码：tymishop
     } while ( u8g2.nextPage() );
}
