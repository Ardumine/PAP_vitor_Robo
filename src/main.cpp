#include <Arduino.h>
#include <ArduinoJson.h>
#include <NeoSWSerial.h>
#include <classes.h>
#include <List.hpp>

#include <Wire.h>

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

void LOG(String dados)
{
  //Serial.println(dados);
}

void Update_prox_lugar();

#pragma region Parte COM
String inputString = "";
List<String> Dados_Recs;
String string_Rec_incomp = "";
bool stringComplete = false;

NeoSWSerial Serial_MC(8, 9);

void Mandar_p_MB_raw(String dados)
{

  Serial_MC.println((dados.c_str()));
  // Serial_MC.println("");
  //  LOG("M: " + dados);
}

void Mandar_dados_PC_raw(String dados)
{
  Mandar_p_MB_raw("pr_pc" + dados);
  // LOG(dados);
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
  Serial.begin(9600); // 115200

  // Serial_MC.begin(9600);

  Serial_MC.attachInterrupt(evento_Serial_mc);
  Serial_MC.begin(9600);

  LOG("INI");
  for (size_t i = 0; i < 20; i++)
  {
    delay(50);
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
  }
  LOG("INI INO");
  // Update_dados();
  Update_prox_lugar();
  Serial.setTimeout(300);
}

bool Houve_update_linhas = true;

int vel_seg1 = 60;
int vel_seg2 = 0;

bool Ativar_Seguir_linha = true;

int vel_m1 = 0;
int vel_m2 = 0;

bool estado_ant_L2 = true; // Tem de ser true ou kaboom *Sons de explosão*
int linhas_passadas1 = 1;  // ident chefe Linhas passadas no chefe.

bool estado_ant_R2 = true; // Tem de ser true ou kaboom *Sons de explosão*
int linhas_passadas2 = 0;  // Linhas passadas nas mesas

bool Calibrado = false;

bool Btn_b_pressionado = false;
float dist_ultra = 0;


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

  Btn_b_pressionado = dados_Rec.Estado_btn;
  dist_ultra = dados_Rec.Dist_ultra;
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

  LOG("rec pc: " + (dados_raw));
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
      LOG("ERRO JSON");
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

    LOG("cmd: " + tipo_cmd);

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

void delay_update(int tempo)
{
  int temp_ini = millis();
  interrupts();
  while ((millis() - temp_ini) < tempo)
  {
    // yield();
    //
    if (Serial_MC.available())
    {
      // Mandar_dados_PC_raw("{" + String('"') + "a" + String('"') + ":" + String(Serial_MC.available()) + "}");
    }
    delay(1);
  }
}

void Update_prox_lugar()
{
  LOG("Pedidos:");
  for (int i = 0; i < Mesas_p_ir.getSize(); i++)
  {
    // LOG("  M:");
    // LOG(Mesas_p_ir[i].ID_mesa);
    // LOG("; T:");
    // LOG(Mesas_p_ir[i].tipo_de_ir);
  }

  bool Encontrou = false;
  int id_mesa_crrt = Obter_mesa(Lugar_crrt);

  for (int i = 0; i < Mesas_p_ir.getSize(); i++)
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
    // LOG(" S pedidos. Ir chefe");
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
String cmd_tirar_foto= "tf";



String nome_var_tipo = "t";

String nome_var_calibrado = "c";

String nome_var_linha1 = "l1";
String nome_var_linha2 = "l2";

String nome_var_Lugar_crrt = "lc";
String nome_var_Lugar_obj = "lo";

String nome_var_tipo_ir_mesa = "tim";
String nome_var_ID_mesa = "idm";

String nome_var_pedidos = "ped";

static void evento_Serial_mc(uint8_t c)
{
  inputString += (char)c;

  // if (c_ch != -1)
  //{
  //  LOG(inChar);
  //}
  if (c == '\n')
  {
    stringComplete = true;
  }
}

List<int> Numeros_rec_camera;
bool Acabou_ler_camera = false;

void Dado_recebido_Camera(int dado)
{
  if (dado == 11)
  {
    Acabou_ler_camera = true;
    Serial.println("FIM!");
  }
  else
  {
    Numeros_rec_camera.add(dado);
  }
  Serial.println(dado);
}

void Task_update_camera()
{
  if (Serial.available())
  {

    String rec_ = Serial.readString();
    rec_.replace("\n", "");
    rec_.replace("\r", "");
    int rec = rec_.toInt();

    Dado_recebido_Camera(rec);
  }
}

void Mandar_stats()
{

  StaticJsonDocument<100> JSON_recebido;

  JSON_recebido[nome_var_tipo] = cmd_obter_stats;

  // JSON_recebido[nome_var_linha1] = linhas_passadas1;
  // JSON_recebido[nome_var_linha2] = linhas_passadas2;

  JSON_recebido[nome_var_Lugar_crrt] = (int)(Lugar_crrt);
  JSON_recebido[nome_var_Lugar_obj] = (int)(Lugar_objetivo);

  // JSON_recebido[nome_var_calibrado] = Calibrado ? 1 : 0;
  String oute = "";
  serializeJson(JSON_recebido, oute);
  Mandar_dados_PC_raw(oute);
  LOG(oute);
}

void taskSerial()
{

  if (stringComplete)
  {
    // delay(100);
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));

    if (inputString.startsWith("de_pc"))
    {

      Parar_seguir_linha();
      inputString.replace("de_pc", "");
      inputString.replace("\n", "");
      inputString.replace("\r", "");

      if (inputString.startsWith("e"))
      {
        Mandar_dados_PC_raw("OK");
      }
      else if (inputString.startsWith("{"))
      {

        StaticJsonDocument<80> JSON_recebido; // 80

        DeserializationError error = deserializeJson(JSON_recebido, inputString);

        if (error)
        {
          LOG("ERRO JSON");
          LOG(inputString);

          Mandar_dados_PC_raw("EP");
          return;
        }

        String tipo_cmd = JSON_recebido[nome_var_tipo]; //.as<String>()

        LOG("cmd: " + tipo_cmd);

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
          bool existe = false;

          for (int i = 0; i < Mesas_p_ir.getSize(); i++) // Serve para verificar se pedido de ir n existe
          {
            /* code */
            Pedido_ir_mesa p_c = Mesas_p_ir.getValue(i);
            if (pedido.ID_mesa == p_c.ID_mesa && pedido.tipo_de_ir == p_c.tipo_de_ir)
            {
              existe = true;
            }
          }
          if (!existe)
          {
            Mesas_p_ir.add(pedido);
          }
          Houve_update_linhas = true;
          Update_prox_lugar();
        }
        else if (tipo_cmd == cmd_tirar_foto){
          Serial.println("ft");
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
      LOG("");
      LOG("INFO OK");
      Espera_de_info_tempo = false;
    }
    ultimo_tempo_rec_info = millis();
    // LOG("R: " + inputString);

    inputString = "";
    stringComplete = false;
  }

  if ((millis() - ultimo_tempo_rec_info) > 500)
  {
    if (Espera_de_info_tempo)
    {
      LOG(".");
      digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    }
    else
    {
      LOG("A mandar ped info...");
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

bool A_fazer_coisas_numa_mesa = false;

List<int> Pedidos_da_mesa_ler;

void loop()
{

  Task_update_camera();
  int tempo_ant = millis();

  // LOG(String(millis() - tempo_ant));
  if (A_fazer_coisas_numa_mesa)
  {
    if (pedido_crrt.tipo_de_ir == Tipo_ir_mesa::PEntregar_comida)
    {
      if (Btn_b_pressionado)
      {

        LOG("Acabou de entregar!");

        A_fazer_coisas_numa_mesa = false;
        Houve_update_linhas = true;

        
        A_fazer_coisas_numa_mesa = false;
        Houve_update_linhas = true;
        
      }
    }
    else
    {

      if (Acabou_ler_camera)
      {

        const int CAPACITY = JSON_ARRAY_SIZE(Numeros_rec_camera.getSize());

        DynamicJsonDocument doc(CAPACITY);

        JsonArray array = doc.to<JsonArray>();

        for (int i = 0; i < Numeros_rec_camera.getSize(); ++i)
        {
          array.add((int)(Numeros_rec_camera.getValue(i) + 0));
        }

        StaticJsonDocument<100> JSON_recebido;

        JSON_recebido.clear();

        JSON_recebido[nome_var_tipo] = cmd_rec_Pedidos;

        JSON_recebido[nome_var_ID_mesa] = pedido_crrt.ID_mesa;

        JSON_recebido[nome_var_pedidos] = array;

        String oute = "";
        serializeJson(JSON_recebido, oute);
        Mandar_dados_PC_raw(oute);
        LOG(oute);

        LOG("Acabou de obter pedidos!");

        Numeros_rec_camera.clear();

        A_fazer_coisas_numa_mesa = false;
        Houve_update_linhas = true;
        Acabou_ler_camera = false;
      }
    }
  }
  if (Houve_update_linhas)
  {
    LOG("UPDATE");
    Parar_seguir_linha();
    Parar_seguir_linha();
    Parar_seguir_linha();

    Lugar_crrt = Obter_lugar_crrt(true);
    Mandar_stats();

    if (!A_fazer_coisas_numa_mesa)
    {
      if (Lugar_objetivo == Lugar_crrt) //
      {
        Mesas_p_ir.remove(idx_pedido_crt);
        LOG("Removido!");
        Parar_seguir_linha();
        if (Lugar_objetivo == Zona_chefe)
        {
          Parar_seguir_linha();
          LOG("No chefe!");
        }
        else
        {
          LOG("Numa mesa!");
          A_fazer_coisas_numa_mesa = true;
          LOG("A fazer coisas...");

          // Ativar_Seguir_linha = true;
        }
      }
      else
      {
        Ativar_Seguir_linha = true;
        LOG("A ir para objetivo...");
        LOG((String)Lugar_objetivo);
        LOG((String)Lugar_crrt);

        //
      }

      Update_prox_lugar();

      if (Mesas_p_ir.getSize() == 0)
      {
        LOG("Sem pedidos.");
        if (Lugar_objetivo != Zona_chefe)
        {
          LOG("A ir para o chefe...");
          Lugar_objetivo = Zona_chefe;
          Houve_update_linhas = true;
        }
      }
    }

    Houve_update_linhas = false;
  }

  taskSerial();

}