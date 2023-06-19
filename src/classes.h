#include <Arduino.h>
#include<tools.h>

class Dados_mandar_MC_controlo
{
private:
    /* data */
public:
   int  Vel_motor_1 = 0;
   int Vel_motor_2 = 0;
};


class Dados_rec_MC
{
private:
    /* data */
public:
    void A_partir_de_string(String dados){
        Dist_ultra = getValue(dados,';',0).toFloat();
        SensorL1 = getValue(dados,';',1).toInt() == 1;
        SensorM = getValue(dados,';',2).toInt() == 1;
        SensorR1 = getValue(dados,';',3).toInt() == 1;
        SensorL2 = getValue(dados,';',4).toInt() == 1;
        SensorR2 = getValue(dados,';',5).toInt() == 1;
        Tam_cache_mb = getValue(dados,';',6).toInt();

        
    }
    float Dist_ultra = 0;
    bool SensorL1 = 0;
    bool SensorM = 0;
    bool SensorR1 = 0;
    bool SensorL2 = 0;
    bool SensorR2 = 0;
    int Tam_cache_mb = 0;
};



