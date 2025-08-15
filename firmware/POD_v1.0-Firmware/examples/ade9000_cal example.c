void calibrate_ade9000() {

  delay(10000); //wait for RMS to settle

  // enter values
  uint16_t V_TEST = 220;//volts RMS
  uint16_t I_TEST = 141.4;//Amps RMS
  double Power_factor = 1;
  int meter_constant = 1000;  //1000 imp/Kwh for cf output rate
  int Line_Freq = 50;
  int accumulation_time = 3; //sec
  int number_of_channel_in_cf = 2; // 2 for 2 channel 3 for all 3 

  double R1 = 1000000; //big resistor or sum of upper resistors
  double R2 = 1000; //small resistor

  double CT_Ratio = 2000; // to 1
  double burden = 2.5*2; // 2 resistors differential burdens or 1 single burden resistor for single ended.
  
  //constants
#define  PMAX 20694066     
#define  Sample_Rate 8000
#define  WTHR_Value 1048576    //WTHR = 0x00100000 = 1048576
#define  V_PGA  1
#define  I_PGA  1
#define  I_channel_ADC_FS  (.707 / I_PGA)
#define  V_channel_ADC_FS  (.707 / V_PGA)
#define  RMS_FS_Codes 52702092        



  //calculated
  double Percent_FS_Voltage = (((R2 / (R1 + R2)) * V_TEST) / V_channel_ADC_FS);
  double Percent_FS_Current = ((I_TEST / CT_Ratio) * burden ) / I_channel_ADC_FS;
  double V_Fullscale = V_TEST / Percent_FS_Voltage;
  double I_Fullscale = I_TEST / Percent_FS_Current;
  double Expected_CF_Freq = (meter_constant / 1000 * V_TEST * I_TEST * Power_factor) / 3600;

  //moved to global
  //double IRMS_LSB = 0;
  //double VRMS_LSB = 0;
  //double WATT_LSB = 0;
  //double KWH_LSB = 0;


  
  
  uint16_t CFDEN_CAL_VALUE = (uint16_t)(((20694066.00 / 1048576.00) * (4096000.00 / 512.00) * Power_factor * Percent_FS_Voltage * Percent_FS_Current) / Expected_CF_Freq); //.00 forces float or double rounding error if not used
  int32_t AIGAIN_CAL_VALUE = (int32_t)((((RMS_FS_Codes * Percent_FS_Current) / (double)ADE_SPI_READ(ADDR_AIRMS, 4)) - 1) * 134217728); // 2^27 = 134,217,728
  int32_t AVGAIN_CAL_VALUE = (int32_t)((((RMS_FS_Codes * Percent_FS_Voltage) / (double)ADE_SPI_READ(ADDR_AVRMS, 4)) - 1) * 134217728); // 2^27 = 134,217,728

  int32_t BIGAIN_CAL_VALUE = (int32_t)((((RMS_FS_Codes * Percent_FS_Current) / (double)ADE_SPI_READ(ADDR_BIRMS, 4)) - 1) * 134217728); // 2^27 = 134,217,728
  int32_t BVGAIN_CAL_VALUE = (int32_t)((((RMS_FS_Codes * Percent_FS_Voltage) / (double)ADE_SPI_READ(ADDR_BVRMS, 4)) - 1) * 134217728); // 2^27 = 134,217,728

  int32_t CIGAIN_CAL_VALUE = (int32_t)((((RMS_FS_Codes * Percent_FS_Current) / (double)ADE_SPI_READ(ADDR_CIRMS, 4)) - 1) * 134217728); // 2^27 = 134,217,728
  int32_t CVGAIN_CAL_VALUE = (int32_t)((((RMS_FS_Codes * Percent_FS_Voltage) / (double)ADE_SPI_READ(ADDR_CVRMS, 4)) - 1) * 134217728); // 2^27 = 134,217,728


  Serial.println("");
  Serial.print("  V_TEST Used For Calibration = "); Serial.println(V_TEST);
  Serial.print("  I_TEST Used For Calibration = "); Serial.println(I_TEST);
  Serial.print("  Power_factor Used For Calibration = "); Serial.println(Power_factor);
  Serial.print("  Line_Freq Used For Calibration = "); Serial.println(Line_Freq);
  Serial.print("  Accumulation Time Used For Calibration = "); Serial.println(accumulation_time);
  Serial.println("");

  Serial.print("  Vlevel Calculated Register Value = "); Serial.println((uint32_t)((V_Fullscale / V_TEST) * 4000000), HEX);
  Serial.println("");

  ADE_SPI_WRITE(ADDR_VLEVEL, (uint32_t)((V_Fullscale / V_TEST) * 1144084), 4);   //0x45D45 * 4 = 1144084
  
  Serial.print("  V_TEST = "); Serial.print(Percent_FS_Voltage * 100, 2); Serial.print("% Fullscale"); Serial.print(" = "); Serial.print(V_channel_ADC_FS * Percent_FS_Voltage,4);Serial.print(" VRMS at ADC input and ");Serial.print(V_Fullscale, 2); Serial.println(" VRMS Fullscale");
  Serial.print("  I_TEST = "); Serial.print(Percent_FS_Current * 100, 2); Serial.print("% Fullscale"); Serial.print(" = "); Serial.print(I_channel_ADC_FS * Percent_FS_Current,4);Serial.print(" VRMS at ADC input and ");Serial.print(I_Fullscale, 2); Serial.println(" IRMS Fullscale");

  Serial.println("");
  Serial.print("  Expected_CF_Freq = "); Serial.println(Expected_CF_Freq, 6);
  Serial.print("  CFDEN_CAL_VALUE = "); Serial.println(CFDEN_CAL_VALUE);
  Serial.println("");

  ADE_SPI_WRITE(ADDR_CF1DEN, CFDEN_CAL_VALUE, 2);
  ADE_SPI_WRITE(ADDR_CF2DEN, CFDEN_CAL_VALUE, 2);
  ADE_SPI_WRITE(ADDR_CF3DEN, CFDEN_CAL_VALUE, 2);
  
  Serial.print("  AIRMS Expected = "); Serial.println(Percent_FS_Current * RMS_FS_Codes ,0);
  Serial.print("  AVRMS Expected = "); Serial.println(Percent_FS_Voltage * RMS_FS_Codes ,0);
  Serial.print("  AIRMS RAW = "); Serial.println(ADE_SPI_READ(ADDR_AIRMS, 4), DEC);
  Serial.print("  AVRMS RAW = "); Serial.println(ADE_SPI_READ(ADDR_AVRMS, 4), DEC);
  Serial.println("");
  Serial.print("  AIGAIN_CAL_VALUE "); Serial.println(AIGAIN_CAL_VALUE);
  Serial.print("  AVGAIN_CAL_VALUE "); Serial.println(AVGAIN_CAL_VALUE);
  Serial.println("");

  ADE_SPI_WRITE(ADDR_AIGAIN, AIGAIN_CAL_VALUE, 4);
  ADE_SPI_WRITE(ADDR_AVGAIN, AVGAIN_CAL_VALUE, 4);
  
  Serial.print("  BIRMS Expected = "); Serial.println(Percent_FS_Current * RMS_FS_Codes ,0);
  Serial.print("  BVRMS Expected = "); Serial.println(Percent_FS_Voltage * RMS_FS_Codes ,0);
  Serial.print("  BIRMS RAW = "); Serial.println(ADE_SPI_READ(ADDR_BIRMS, 4), DEC);
  Serial.print("  BVRMS RAW = "); Serial.println(ADE_SPI_READ(ADDR_BVRMS, 4), DEC);
  Serial.println("");
  Serial.print("  BIGAIN_CAL_VALUE "); Serial.println(BIGAIN_CAL_VALUE);
  Serial.print("  BVGAIN_CAL_VALUE "); Serial.println(BVGAIN_CAL_VALUE);
  Serial.println("");

  ADE_SPI_WRITE(ADDR_BIGAIN, BIGAIN_CAL_VALUE, 4);
  ADE_SPI_WRITE(ADDR_BVGAIN, BVGAIN_CAL_VALUE, 4);


  Serial.print("  CIRMS Expected = "); Serial.println(Percent_FS_Current * RMS_FS_Codes ,0);
  Serial.print("  CVRMS Expected = "); Serial.println(Percent_FS_Voltage * RMS_FS_Codes ,0);
  Serial.print("  CIRMS RAW = "); Serial.println(ADE_SPI_READ(ADDR_CIRMS, 4), DEC);
  Serial.print("  CVRMS RAW = "); Serial.println(ADE_SPI_READ(ADDR_CVRMS, 4), DEC);
  Serial.println("");
  Serial.print("  CIGAIN_CAL_VALUE "); Serial.println(CIGAIN_CAL_VALUE);
  Serial.print("  CVGAIN_CAL_VALUE "); Serial.println(CVGAIN_CAL_VALUE);
  Serial.println("");

  ADE_SPI_WRITE(ADDR_CIGAIN, CIGAIN_CAL_VALUE, 4);
  ADE_SPI_WRITE(ADDR_CVGAIN, CVGAIN_CAL_VALUE, 4);



  ADE_SPI_WRITE(ADDR_EGY_TIME, (accumulation_time / (1.00 / Line_Freq / 2))-1, 2); // 300 half linecycles or 3 sec at 50Hz  360 half line cycles at 60hz for 3 sec dont forget -1

  Serial.print("  LINECYCLES for "); Serial.print(accumulation_time); Serial.print(" SEC accumulation time = "); Serial.println(accumulation_time / (1.00 / Line_Freq / 2), 0);

  IRMS_LSB = I_TEST / (RMS_FS_Codes * Percent_FS_Current);
  VRMS_LSB = V_TEST / (RMS_FS_Codes * Percent_FS_Voltage);
  WATT_LSB = (V_TEST * I_TEST) / (PMAX * Percent_FS_Current * Percent_FS_Voltage);

//<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
  // you can use expected (WATTHR / watthr register read) to calculate KWH/LSB.
  /*
    delay(accumulation_time * 1000);
    //  need to wait for next accumulation time Watthr used for calculation
    ADE7978_SPI_WRITE(STATUS0,32,4);
    while( (ADE7978_SPI_READ(STATUS0, 4) & 32) != 32)
       {
          ADE7978_SPI_WRITE(STATUS0,32,4);
          KWH_LSB = (V_TEST * I_TEST * Power_factor * accumulation_time) / (ADE7978_SPI_READ(AWATTHR,4) * 3600);
       }
  */


  //Or KWH/LSB can be calculated by using expected values and the signal chain. (watthr over accumulation time) / (calculated watthr register over accumulation time )// note accumulation time cancels out I left it in for clarity

//pow(2,13) = 8192 is because the  watthr_hi is shifted 13 bit to the  right   pg 21 fig 24 https://www.analog.com/media/en/technical-documentation/user-guides/ADE9000-UG-1098.pdf
  WH_LSB = ((double)(((V_TEST * I_TEST * Power_factor) / 3600.00) * accumulation_time) / ((double)(PMAX * Percent_FS_Current * Percent_FS_Voltage  * 8000.00 / 8192.00 * accumulation_time ))); // (watthr over accumulation time) / (calculated watthr register over accumulation time )
  //Serial.print("calculated KWH/LSB = "); Serial.println(KWH_LSB, 8);
//<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
  
 

  Serial.println("");
  Serial.print("  IRMS_LSB = "); Serial.println(IRMS_LSB, 8);
  Serial.print("  VRMS_LSB = "); Serial.println(VRMS_LSB, 8);
  Serial.print("  WATT_LSB = "); Serial.println(WATT_LSB, 8);
  Serial.print("    WH_LSB = ");  Serial.println(WH_LSB, 8);
  Serial.println("");

  delay(10000);

  Serial.println("  Cf will measure 3 x expected CF if 3 phases are preset at same V and I and PF");
  Serial.print("  Cf Frequency Measured By Micro = "); Serial.print(cf_freq, 6); Serial.print(" HZ    error from expected = ");Serial.print(((cf_freq / (Expected_CF_Freq * number_of_channel_in_cf) )-1.00) * 100.00,6);Serial.println("%");
  Serial.println("");
  Serial.println("");
  while (1) {
    if ( (ADE_SPI_READ(ADDR_STATUS0, 4) & 1) == 1)
    {
      ADE_SPI_WRITE(ADDR_STATUS0, 1, 4);
      
      Serial.print("  Updated Every "); Serial.print(accumulation_time / (1.00 / Line_Freq / 2),0); Serial.println(" Half Lincycles ");
      Serial.println("");
      Serial.print("  AIRMS = "); Serial.print(ADE_SPI_READ(ADDR_AIRMS, 4) * IRMS_LSB,4); Serial.println(" IRMS");
      Serial.print("  AVRMS = "); Serial.print(ADE_SPI_READ(ADDR_AVRMS, 4) * VRMS_LSB,4); Serial.println(" VRMS");
      Serial.print("  AWATT = "); Serial.print(ADE_SPI_READ(ADDR_AWATT, 4) * WATT_LSB,4); Serial.println(" WATTS"); //div by 2^4 in figure 71 pg 51
      //https://www.analog.com/media/en/technical-documentation/data-sheets/ade7854a_7858a_7868a_9000a.pdf
      Serial.print("  AWATTHR = "); Serial.print(ADE_SPI_READ(ADDR_AWATTHR_HI, 4) * WH_LSB/1000,6); Serial.println(" KW/HR");
      Serial.println("");
      Serial.print("  BIRMS = "); Serial.print(ADE_SPI_READ(ADDR_BIRMS, 4) * IRMS_LSB,4); Serial.println(" IRMS");
      Serial.print("  BVRMS = "); Serial.print(ADE_SPI_READ(ADDR_BVRMS, 4) * VRMS_LSB,4); Serial.println(" VRMS");
      Serial.print("  BWATT = "); Serial.print(ADE_SPI_READ(ADDR_BWATT, 4) * WATT_LSB,4); Serial.println(" WATTS"); //div by 2^4 in figure 71 pg 51
      //https://www.analog.com/media/en/technical-documentation/data-sheets/ade7854a_7858a_7868a_9000a.pdf
      Serial.print("  BWATTHR = "); Serial.print(ADE_SPI_READ(ADDR_BWATTHR_HI, 4) * WH_LSB / 1000,6); Serial.println(" KW/HR");
      Serial.println("");
      Serial.print("  CIRMS = "); Serial.print(ADE_SPI_READ(ADDR_CIRMS, 4) * IRMS_LSB,4); Serial.println(" IRMS");
      Serial.print("  CVRMS = "); Serial.print(ADE_SPI_READ(ADDR_CVRMS, 4) * VRMS_LSB,4); Serial.println(" VRMS");
      Serial.print("  CWATT = "); Serial.print(ADE_SPI_READ(ADDR_CWATT, 4) * WATT_LSB,4); Serial.println(" WATTS"); 
      Serial.print("  CWATTHR = "); Serial.print(ADE_SPI_READ(ADDR_CWATTHR_HI, 4) * WH_LSB / 1000,6); Serial.println(" KW/HR");
      Serial.println("");
    }
  }
}