#include <HardwareSerial.h>

HardwareSerial mySerial(1); // UART1

const int TAILLE_TRAME = 25; //taille de la trame
const int TAILLE_TRAME_COMPLETE = 26;// pret a recevoir jusqu'100 octets  pour éviter dépassement

const unsigned long TEMPS_ENTRE_TRAMES = 10000; // 10 secondes d’attente entre deux trames
const unsigned long TIMEOUT_RECEPTION = 1500;    // un timer de 150 ms est lancé juste aprés la reception du premier caractére 

byte trame[TAILLE_TRAME];// tableau pour stocker les  25 octets
byte trameComplete[TAILLE_TRAME_COMPLETE]; // tableau pour stocker  tous les octets recus(le total)
int indexTrame = 0; // Compteur nombre d'octets stockés dans "trame" 
int indexComplete = 0;  // Compteur nombre d'octets stockés dans "trameComplete" 

hw_timer_t *timer = NULL;
volatile uint8_t flag_timer_0 = 0;

char memo_car;//Variable temporaire pour stocker l'octet lu

unsigned long compteur10s = 0; // Compteur au cas au on ne recois pas de trame
const unsigned long LIMITE_10S = TEMPS_ENTRE_TRAMES / 100; 
bool erreur10sEnvoyee = false;

// Fonction qui permet de vider le buffer  (calculer et retourner le nombre d'octets a supprimer )
int viderBuffer() {
  int count = 0;
  while (mySerial.available()) {
    mySerial.read();
    count++;
  }
  return count;
}

// fonction pour  vider le buffer et affiche rle nombre d'octets supprimés
void viderEtAfficherBuffer(int totalRecu) {
  int vidage = viderBuffer();
  Serial.print("****************** Vider le buffer UART (");
  Serial.print(totalRecu);
  Serial.println(" octets supprimés).");
}

// affichage des octets en hexadecimal  
void afficherTrame(byte trame[], int taille) {
  Serial.println("\n Trame reçue :");
  for (int i = 0; i < taille; i++) {
    Serial.print("Octet ");
    Serial.print(i);
    Serial.print(": 0x");
    if (trame[i] < 0x10) Serial.print("0");
    Serial.print(trame[i], HEX);
    Serial.println();
  }
}

// ISR Timer
void IRAM_ATTR timer_isr() {
  flag_timer_0 = 1;
}

void setup() {
  Serial.begin(115200);
  mySerial.begin(115200, SERIAL_8N1, 32,26); // RX=GPI32, TX=GPI26

  // Configuration du timer matériel
  timer = timerBegin(1000000);            // Initialise le timer matériel à 1 MHz
  timerAttachInterrupt(timer, &timer_isr); // Attache la fonction ISR au timer
  timerAlarm(timer, 1501, false, 0);      
  timerStop(timer);                        // Le timer se declanche à la réception du 1er octet
}

void loop() {
  // Réception des octets UART
  while (mySerial.available()) {
    Serial.print("*");                    // Indicateur réception octet
    memo_car = mySerial.read();

    compteur10s = 0;                      // Reset compteur 10s
    erreur10sEnvoyee = false;             // Réarmement erreur 10s
   // Si c'est le tout premier octet reçu
    if (indexComplete == 0) {
      timerWrite(timer, 0);
      timerStart(timer);
    }

    if (indexComplete < TAILLE_TRAME_COMPLETE) {
      trameComplete[indexComplete++] = memo_car;
    }

    if (indexTrame < TAILLE_TRAME) {
      trame[indexTrame++] = memo_car;
    }
  }

  //  Cas ou 150 ms écoulés sans la recpetion de la trame
  if (flag_timer_0) {
    flag_timer_0 = 0;// Réinitialise le flag
    timerStop(timer);

    Serial.println("\n============================");
    Serial.println("*************** Trame reçue , traitement et verification de la trame reçue:");
     //cas ou Aucun octet reçu pendant les 150ms
    if (indexTrame == 0) {
      Serial.println("***************Aucune donnée reçue.");
    } 
    else 
    {
      // Affichage de tous les octets reçus 
      afficherTrame(trameComplete, indexComplete);
      //cas ou on recoit une trame valide de 25 octets 
      if (indexTrame == TAILLE_TRAME)
       {
        //verification du start et stop
        bool startOk = (trame[0] == 0x0F);
        bool stopOk = (trame[TAILLE_TRAME - 1] == 0x00);
         // Affiche la trame stockée
        Serial.println("\n Trame stockée (25 premiers octets) :");
        afficherTrame(trame, TAILLE_TRAME);
        //si les deux le bits(start=0f /stop=00)sont verifiés 
        if (startOk && stopOk) {
          Serial.println("****************** Trame VALIDE.");

           int channels[10]; // Tableau des 10 canaux
           int essaisStart = 0; // Pour debug ou statistique éventuelle

        // Déconcaténation des 10 canaux à partir des octets 1 à 13
        channels[0] = ((trame[1]     | trame[2]  << 8) & 0x07FF);
        channels[1] = ((trame[2] >> 3 | trame[3]  << 5) & 0x07FF);
        channels[2] = ((trame[3] >> 6 | trame[4]  << 2 | trame[5] << 10) & 0x07FF);
        channels[3] = ((trame[5] >> 1 | trame[6]  << 7) & 0x07FF);
        channels[4] = ((trame[6] >> 4 | trame[7]  << 4) & 0x07FF);
        channels[5] = ((trame[7] >> 7 | trame[8]  << 1 | trame[9] << 9) & 0x07FF);
        channels[6] = ((trame[9] >> 2 | trame[10] << 6) & 0x07FF);
        channels[7] = ((trame[10] >> 5 | trame[11] << 3) & 0x07FF);
        channels[8] = ((trame[12]     | trame[13] << 8) & 0x07FF);
        channels[9] = ((trame[13] >> 3 | trame[14] << 5) & 0x07FF);

        // Affichage des canaux
        Serial.println("****************** Déconcaténation des canaux :");
          for (int i = 0; i < 10; i++) {
              Serial.print("Channel ");
              Serial.print(i);
              Serial.print(" : ");
              Serial.print(": 0x");
              Serial.println(channels[i], HEX); // ou HEX si tu préfères
               }

        }
         //SI LE BIT DE START ET/OU DE STOP NE SONT PAS verifiés , la trame est invalide 
         else 
         {
          Serial.println("********************** Trame invalide :");
          if (!startOk) Serial.println("  - Start byte incorrect (≠ 0x0F)");
          if (!stopOk) Serial.println("  - Stop byte incorrect (≠ 0x00)");
        }
         // cas ou on envoie une trame >25 octets 
        if (indexComplete > TAILLE_TRAME) 
        {
          Serial.print("****************** Trame très longue (");
          Serial.print(indexComplete);
          Serial.println(" octets reçus) : dépassement de la taille maximale de 25 octets.");
        }

        Serial.print("\n****************** Trame reçue : ");
        Serial.print(TAILLE_TRAME);
        Serial.print(" octets stockés, ");
        int ignores = indexComplete - TAILLE_TRAME;
        if (ignores < 0) ignores = 0;
        Serial.print(ignores);
        Serial.println(" octets ignorés.");

        viderEtAfficherBuffer(indexComplete);// appel a la focntion

         // cas ou on recoit une trame incomplète (moins de 25 octets reçus)
      } 
      else
      {
        Serial.print("***********Trame incomplète : ");
        Serial.print(indexTrame);
        Serial.print(" / ");
        Serial.print(TAILLE_TRAME);
        Serial.println(" octets reçus.");
        afficherTrame(trame, indexTrame);
        Serial.println("************* Trame invalide et incomplète.");
     
        Serial.print("\n****************** Trame reçue : ");
        Serial.print(indexTrame);
        Serial.print(" octets stockés, 0 octets ignorés.\n");// on stocke juste les octets recus , avant que le timer tombe

        viderEtAfficherBuffer(indexComplete);
      }
    }

    // Reset index pour nouvelle réception
    indexTrame = 0;
    indexComplete = 0;
  }

  // Gestion erreur 10 secondes sans réception
  static unsigned long dernierTick = 0;// variable pour memoriser l 'absence d une reception
  if (millis() - dernierTick >= 100) {
    dernierTick = millis();
    if (!erreur10sEnvoyee) {
      compteur10s++;
      if (compteur10s >= LIMITE_10S) {
        Serial.println("\n============================");
        Serial.println("***************Aucune trame reçue depuis 10 secondes.");
        int vidage = viderBuffer();
        Serial.print("*************** Buffer  deja vide (");
        Serial.print(vidage);
        Serial.println(" octets supprimés).");
        erreur10sEnvoyee = true;
      }
    }
  }
}
