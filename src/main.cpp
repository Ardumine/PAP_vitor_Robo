#include <Arduino.h>
#include "HUSKYLENS.h"
// #include <SoftwareSerial.h>
#include <ArduinoJson.h>
#include <NeoSWSerial.h>
#include <classes.h>
#include <List.hpp>

enum Lugares
{
  Desconhecido,

  Zona_chefe,

  Mesa_1_Obter_pedido,
  Mesa_1_entrega,

  Mesa_2_Obter_pedido,
  Mesa_2_entrega,

  Mesa_3_Obter_pedido,
  Mesa_3_entrega,

  Andar
};

void Update_prox_lugar();

#pragma region Parte COM
String inputString = "";
bool stringComplete = false;

HUSKYLENS huskylens;
// NeoSWSerial COM_huskylens(4, 5); // RX, TX
NeoSWSerial Serial_MC(2, 3);

void Mandar_p_MB_raw(String dados)
{

  Serial_MC.println(dados);
  // Serial_MC.println("");
  //  Serial.println("M: " + dados);
}

void Mandar_dados_PC_raw(String dados)
{
  Mandar_p_MB_raw("pr_pc" + dados);
  // Serial.println(dados);
  // delay(300);
}

void Update_dados()
{
  Mandar_p_MB_raw("info");
}

static void evento_Serial_mc(uint8_t c);

#pragma endregion

void setup()
{
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(115200);

  // COM_huskylens.begin(9600);

  // Serial_MC.begin(9600);
  Serial_MC.attachInterrupt(evento_Serial_mc);
  Serial_MC.begin(9600);
  // Wire.begin();
  // while (!huskylens.begin(Wire))
  {
    Serial.println(F("Begin failed!"));
    Serial.println(F("2.Please recheck the connection."));
    delay(500);
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    //  break;
  }

  // huskylens.writeAlgorithm(ALGORITHM_TAG_RECOGNITION); // Switch the algorithm to object tracking.

  Serial.println("INI");
  for (size_t i = 0; i < 10; i++)
  {
    delay(100);
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
  }
  Serial.println("INI INO");
  // Update_dados();
  Update_prox_lugar();
}

void Resultado_recebido(HUSKYLENSResult result)
{

  if (result.command == COMMAND_RETURN_BLOCK)
  {
    // https://dfimg.dfrobot.com/nobody/wiki/400e09570d1ff647bd5e24da4ef4a0f9.png

    //  Serial.println(String() + F("X:") + result.xCenter + F(" Y:") + result.yCenter + F(" W:") + result.width + F(" H:") + result.height + F(" ID:") + result.ID + F(" Area:") + result.width * result.height) ;

    String texto = "";
    texto += String(result.width); // 0
    texto += ",";
    texto += String(result.height); // 1
    texto += ",";

    texto += String(result.xCenter); // 2
    texto += ",";
    texto += String(result.yCenter); // 3
    texto += ",";

    texto += String(result.ID); // 4

    Serial.println(texto);

    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
  }
  else
  {
    Serial.println("Object unknown!");
  }
}

bool Houve_update_linhas = true;

int vel_seg1 = 30;
int vel_seg2 = 0;

bool Ativar_Seguir_linha = true;

int vel_m1 = 0;
int vel_m2 = 0;

bool estado_ant_L2 = true; // Tem de ser true ou kaboom *Sons de explosão*
int linhas_passadas1 = 1;  // ident chefe Linhas passadas no chefe.

bool estado_ant_R2 = true; // Tem de ser true ou kaboom *Sons de explosão*
int linhas_passadas2 = 0;  // Linhas passadas nas mesas

bool Calibrado = false;

enum Tipo_ir_mesa
{
  PEntregar_comida,
  PReceber_pedidos
};

enum Estado
{
  Receber_pedidos,
  Entregar_Pedidos,

  Receber_chefe,
  Seguir_linha
};

struct Pedido_ir_mesa
{
  Tipo_ir_mesa tipo_de_ir;
  int ID_mesa;
};

Estado estado_crrt = Receber_chefe;
Lugares Lugar_crrt = Lugares::Zona_chefe;

Lugares Lugar_objetivo = Lugares::Zona_chefe;

List<Pedido_ir_mesa> Mesas_p_ir;

Pedido_ir_mesa pedido_crrt;

int idx_pedido_crt = 0;

Lugares Obter_lugar_crrt(bool skip_andar = false)
{                                 // Obter lugar crrt a partir das linhas
  int linhas1 = linhas_passadas1; // Linhas passadas chefe
  int linhas2 = linhas_passadas2; // Linhas passadas chefe

  if (Ativar_Seguir_linha && !skip_andar)

    return Andar;

  if (linhas1 >= 1)
  {
    linhas_passadas1 = 0; // Resetar tudo
    linhas_passadas2 = 0;

    return Zona_chefe;
  }
  switch (linhas2)
  {
  case 1:
    return Mesa_1_Obter_pedido;
  case 2:
    return Mesa_1_entrega;

  case 3:
    return Mesa_2_Obter_pedido;
  case 4:
    return Mesa_2_entrega;

  case 5:
    return Mesa_3_Obter_pedido;
  case 6:
    return Mesa_3_entrega;

  default:
    return Desconhecido;
  }
}

int Obter_mesa(Lugares lugar)
{

  switch (lugar)
  {
  case Mesa_1_Obter_pedido:
    return 1;
  case Mesa_1_entrega:
    return 1;

  case Mesa_2_Obter_pedido:
    return 2;
  case Mesa_2_entrega:
    return 2;

  case Mesa_3_Obter_pedido:
    return 3;
  case Mesa_3_entrega:
    return 3;

  default:
    return 0;
  }
}

#pragma region Parte seguir linha e controlo motores
void Task_seguir_linha(Dados_rec_MC dados_Rec)
{
  // vel_m1 = 0;
  // vel_m2 = 0;

  // 1 branco                   0 preto
  if (dados_Rec.SensorL1 && dados_Rec.SensorR1)
  {
    vel_m1 = vel_seg1;
    vel_m2 = vel_seg1;
  }
  else if (dados_Rec.SensorL1 && !dados_Rec.SensorR1)
  {
    vel_m1 = vel_seg1;
    vel_m2 = vel_seg2;
  }
  else if (!dados_Rec.SensorL1 && dados_Rec.SensorR1)
  {
    vel_m1 = vel_seg2;
    vel_m2 = vel_seg1;
  }
}

void Update_contadores(Dados_rec_MC dados_Rec)
{
  if (dados_Rec.SensorR2 != estado_ant_R2)
  {
    if (dados_Rec.SensorR2)
    {
      linhas_passadas2++;
      Houve_update_linhas = true;
    }
    estado_ant_R2 = dados_Rec.SensorR2;
  }

  if (dados_Rec.SensorL2 != estado_ant_L2)
  {
    if (dados_Rec.SensorL2)
    {
      linhas_passadas1++;
      Houve_update_linhas = true;
      Calibrado = true;
    }
    estado_ant_L2 = dados_Rec.SensorL2;
  }
}

String ult_dados_mandados_motores = "";
void Mandar_dados_motores_MC()
{
  String novos_dados = String(vel_m1) + ";" + String(vel_m2) + ";" + String(linhas_passadas2) + ";";
  // if (ult_dados_mandados_motores != novos_dados)
  //{
  Mandar_p_MB_raw(novos_dados);
  ult_dados_mandados_motores = novos_dados;
  //}
}

void Parar_seguir_linha()
{
  Ativar_Seguir_linha = false;
  vel_m1 = 0;
  vel_m2 = 0;
  Mandar_dados_motores_MC();
  Mandar_dados_motores_MC();
}

void Dados_recebidos_MC(String dados_raw)
{
  Dados_rec_MC dados_Rec;
  dados_Rec.A_partir_de_string(inputString);

  if (dados_Rec.Tam_cache_mb > 5)
  {
    delay(50 * dados_Rec.Tam_cache_mb);
  }
  Update_contadores(dados_Rec);

  if (Ativar_Seguir_linha)
  {
    Task_seguir_linha(dados_Rec);
  }
  else
  {
    vel_m1 = 0;
    vel_m2 = 0;
  }
  Mandar_dados_motores_MC();

  Update_dados();
}

#pragma endregion
// int Lugar_crrt = 1; // 1 ou 2 ou 3

/*
void Dados_recebidos_PC(String dados_raw)
{

  // Mandar_p_MB_raw("");
  // Mandar_p_MB_raw("\n");
  // Mandar_p_MB_raw("\n");

  Serial.println("rec pc: " + (dados_raw));
  Mandar_p_MB_raw("");

  // Mandar_dados_PC_raw("D_OK" + String(dados_raw.length()));
  // Mandar_dados_PC_raw(dados_raw);

  if (dados_raw.startsWith("e"))
  {
    Mandar_dados_PC_raw("OK");
  }
  else if (dados_raw.startsWith("deb_"))
  {
  }
  else if (dados_raw.startsWith("{"))
  {
    StaticJsonDocument<300> JSON_recebido;
    StaticJsonDocument<300> JSON_resposta;

    // JSON_resposta["r"] = 1;

    DeserializationError error = deserializeJson(JSON_recebido, dados_raw);

    // Test if parsing succeeds.
    if (error)
    {
      Serial.println("ERRO JSON");
      Mandar_dados_PC_raw("EP");
      return;
    }

    String cmd_obter_linhas = "obt_l";
    String cmd_set_linhas = "set_l";
    String cmd_obter_stats = "st";
    String cmd_adi_ir_mesa = "aim";

    String nome_var_tipo = "t";

    String nome_var_calibrado = "c";

    String nome_var_linha1 = "l1";
    String nome_var_linha2 = "l2";

    String tipo_cmd = JSON_recebido[nome_var_tipo]; //.as<String>()

    JSON_resposta[nome_var_tipo] = tipo_cmd;

    Serial.println("cmd: " + tipo_cmd);

    if (tipo_cmd == cmd_obter_linhas) // obter linhas
    {
      JSON_resposta[nome_var_linha1] = linhas_passadas1;
      JSON_resposta[nome_var_linha2] = linhas_passadas2;

      Mandar_dados_PC_raw(JSON_resposta.as<String>());
    }

    else if (tipo_cmd == cmd_set_linhas)
    {                                                    // set linhas
      linhas_passadas1 = JSON_recebido[nome_var_linha1]; // linhas passadas
      linhas_passadas2 = JSON_recebido[nome_var_linha2]; // linhas passadas
    }
    else if (tipo_cmd == cmd_obter_stats) // obter stats
    {
      JSON_resposta[nome_var_linha1] = linhas_passadas1;
      JSON_resposta[nome_var_linha2] = linhas_passadas2;
      JSON_resposta[nome_var_calibrado] = Calibrado ? 1 : 0;
      Mandar_dados_PC_raw(JSON_resposta.as<String>());
    }
    else if (tipo_cmd == cmd_adi_ir_mesa) // Adicionar mesa p ir
    {
    }
  }
  else if (dados_raw.startsWith("l"))
  { // loopback
    Mandar_dados_PC_raw(dados_raw);
  }

  // delay(300);
}
*/

void Update_prox_lugar()
{
  Serial.println("Pedidos:");
  for (int i = 0; i < Mesas_p_ir.getSize(); i++)
  {
    Serial.print("  M:");
    Serial.print(Mesas_p_ir[i].ID_mesa);
    Serial.print("; T:");
    Serial.println(Mesas_p_ir[i].tipo_de_ir);
  }

  bool Encontrou = false;
  int id_mesa_crrt = Obter_mesa(Lugar_crrt);

  for (size_t i = 0; i < Mesas_p_ir.getSize(); i++)
  {
    Pedido_ir_mesa pedido = Mesas_p_ir.getValue(i);

    int dist_mesa = pedido.ID_mesa - id_mesa_crrt;

    int dist_melhor = pedido_crrt.ID_mesa - id_mesa_crrt;

    if (((dist_melhor > dist_mesa) && (pedido.tipo_de_ir == Tipo_ir_mesa::PReceber_pedidos) && (pedido_crrt.tipo_de_ir != Tipo_ir_mesa::PEntregar_comida)) || Mesas_p_ir.getSize() == 1) // Quanto menor, o melhor.
    {
      idx_pedido_crt = i;

      pedido_crrt = pedido;
      Encontrou = true;
    }
  }

  Lugares lugar_final;

  if (pedido_crrt.ID_mesa == 1)
  {
    if (pedido_crrt.tipo_de_ir == Tipo_ir_mesa::PEntregar_comida)
    {
      lugar_final = Lugares::Mesa_1_entrega;
    }
    else
    {
      lugar_final = Lugares::Mesa_1_Obter_pedido;
    }
  }

  if (pedido_crrt.ID_mesa == 2)
  {
    if (pedido_crrt.tipo_de_ir == Tipo_ir_mesa::PEntregar_comida)
    {
      lugar_final = Lugares::Mesa_2_entrega;
    }
    else
    {
      lugar_final = Lugares::Mesa_2_Obter_pedido;
    }
  }

  if (pedido_crrt.ID_mesa == 3)
  {
    if (pedido_crrt.tipo_de_ir == Tipo_ir_mesa::PEntregar_comida)
    {
      lugar_final = Lugares::Mesa_3_entrega;
    }
    else
    {
      lugar_final = Lugares::Mesa_3_Obter_pedido;
    }
  }

  if (!Encontrou)
  {
    // Lugar_objetivo = Zona_chefe;
    // Serial.println(" S pedidos. Ir chefe");
  }
  else
  {
    Lugar_objetivo = lugar_final;
  }
}

long ultimo_tempo_rec_info = 0; // Ultimo tempo que recebeu info

bool Espera_de_info_tempo = false; // Verdadeiro se ainda não recebeu info após certo tempo.

String cmd_obter_linhas = "obt_l";
String cmd_set_linhas = "set_l";
String cmd_obter_stats = "st";
String cmd_adi_ir_mesa = "aim";
String cmd_rec_Pedidos = "rp";


String nome_var_tipo = "t";

String nome_var_calibrado = "c";

String nome_var_linha1 = "l1";
String nome_var_linha2 = "l2";

String nome_var_Lugar_crrt = "lc";
String nome_var_Lugar_obj = "lo";

String nome_var_tipo_ir_mesa = "tim";
String nome_var_ID_mesa = "idm";

static void evento_Serial_mc(uint8_t c)
{
  inputString += (char)c;

  // if (c_ch != -1)
  //{
  //  Serial.println(inChar);
  //}
  if (c == '\n')
  {
    stringComplete = true;
  }
}

void Mandar_stats()
{
  StaticJsonDocument<100> JSON_recebido;

  JSON_recebido[nome_var_tipo] = cmd_obter_stats;

  JSON_recebido[nome_var_linha1] = linhas_passadas1;
  JSON_recebido[nome_var_linha2] = linhas_passadas2;

  JSON_recebido[nome_var_Lugar_crrt] = (int)(Lugar_crrt);
  JSON_recebido[nome_var_Lugar_obj] = (int)(Lugar_objetivo);

  JSON_recebido[nome_var_calibrado] = Calibrado ? 1 : 0;
  String oute = "";
  serializeJson(JSON_recebido, oute);
  Mandar_dados_PC_raw(oute);
  Serial.println(oute);
}

void taskSerial()
{

  if (stringComplete)
  {
    // delay(100);
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));

    if (inputString.startsWith("de_pc"))
    {
      vel_m1 = 0;
      vel_m2 = 0;
      Mandar_dados_motores_MC();
      inputString.replace("de_pc", "");
      inputString.replace("\n", "");
      inputString.replace("\r", "");

      if (inputString.startsWith("e"))
      {
        Mandar_dados_PC_raw("OK");
      }
      else if (inputString.startsWith("{"))
      {
        // DynamicJsonDocument  JSON_resposta(2048);

        StaticJsonDocument<100> JSON_recebido; // 80

        DeserializationError error = deserializeJson(JSON_recebido, inputString);

        if (error)
        {
          Serial.println("ERRO JSON");
          Serial.println(inputString);

          Mandar_dados_PC_raw("EP");
          return;
        }

        String tipo_cmd = JSON_recebido[nome_var_tipo]; //.as<String>()

        Serial.println("cmd: " + tipo_cmd);

        if (tipo_cmd == cmd_obter_linhas) // obter linhas
        {
          JSON_recebido.clear();
          JSON_recebido[nome_var_tipo] = tipo_cmd;

          JSON_recebido[nome_var_linha1] = linhas_passadas1;
          JSON_recebido[nome_var_linha2] = linhas_passadas2;

          Mandar_dados_PC_raw(JSON_recebido.as<String>());
        }

        else if (tipo_cmd == cmd_set_linhas)
        {                                                    // set linhas
          linhas_passadas1 = JSON_recebido[nome_var_linha1]; // linhas passadas
          linhas_passadas2 = JSON_recebido[nome_var_linha2]; // linhas passadas
        }
        else if (tipo_cmd == cmd_obter_stats) // obter stats
        {
          Mandar_stats();
        }
        else if (tipo_cmd == cmd_adi_ir_mesa) // Adicionar mesa p ir
        {
          Pedido_ir_mesa pedido;
          pedido.ID_mesa = JSON_recebido[nome_var_ID_mesa];
          pedido.tipo_de_ir = (JSON_recebido[nome_var_tipo_ir_mesa] == 1) ? Tipo_ir_mesa::PReceber_pedidos : Tipo_ir_mesa::PEntregar_comida;
          Mesas_p_ir.add(pedido);
          Houve_update_linhas = true;
          Update_prox_lugar();
        }
      }

      // Dados_recebidos_PC(inputString);
    }

    else
    {

      Dados_recebidos_MC(inputString);
    }

    if (Espera_de_info_tempo)
    {
      Serial.println();
      Serial.println("INFO OK");
      Espera_de_info_tempo = false;
    }
    ultimo_tempo_rec_info = millis();
    // Serial.println("R: " + inputString);

    inputString = "";
    stringComplete = false;
  }

  if ((millis() - ultimo_tempo_rec_info) > 500)
  {
    if (Espera_de_info_tempo)
    {
      Serial.print(".");
    }
    else
    {
      Serial.print("A mandar ped info...");
    }
    ult_dados_mandados_motores = "";
    Update_dados();
    // Mandar_p_MB_raw("");
    ultimo_tempo_rec_info = millis();
    Espera_de_info_tempo = true;
  }
}

void Update_lgr_crrt()
{
}

void loop()
{
  if (Houve_update_linhas)
  {
    Serial.println("UPDATE");
    Parar_seguir_linha();
    Parar_seguir_linha();
    Parar_seguir_linha();


    Lugar_crrt = Obter_lugar_crrt(true);

    if (Lugar_objetivo == Lugar_crrt) //
    {
      Mandar_stats();
      Mesas_p_ir.remove(idx_pedido_crt);
      Serial.println("Removido!");

      Parar_seguir_linha();
      if (Lugar_objetivo == Zona_chefe)
      {
        Parar_seguir_linha();
        Serial.println("No chefe!");
      }
      else
      {
        Serial.println("Numa mesa!");
        Serial.println("A fazer coisas...");

        delay(5000);
        Serial.println("OK");
        // Ativar_Seguir_linha = true;
        
      }
    }
    else
    {
      Ativar_Seguir_linha = true;
      Serial.println("A ir para objetivo...");
      Serial.println(Lugar_objetivo);
      Serial.println(Lugar_crrt);

      //
    }
    Update_prox_lugar();

    Houve_update_linhas = false;

    if (Mesas_p_ir.getSize() == 0)
    {
      Serial.println("Sem pedidos.");
      if (Lugar_objetivo != Zona_chefe)
      {
        Serial.println("A ir para o chefe...");
        Lugar_objetivo = Zona_chefe;
        Houve_update_linhas = true;
      }
    }
  }

  taskSerial();

  // if (!huskylens.request())
  // {
  //   Serial.println(F("Erro com huskylens!"));
  // }
  // else if (!huskylens.isLearned())
  // {
  //   Serial.println(F("Nothing learned, press learn button on HUSKYLENS to learn one!"));
  // }
  // // else if (!huskylens.available())
  // //   Serial.println(F("No block or arrow appears on the screen!"));
  // else
  // {
  //   while (huskylens.available())
  //   {
  //     HUSKYLENSResult result = huskylens.read();
  //     Resultado_recebido(result);
  //   }
  // }
}