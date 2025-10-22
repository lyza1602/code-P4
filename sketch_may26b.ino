#include <WiFi.h>                // Gère la connexion WiFi
#include <WiFiClientSecure.h>   // Gère les connexions TLS

// Paramètres de configuration WiFi 
const char* ssid = "Oriance";                // Nom du réseau WiFi
const char* password = "*eiscom#oriance%";   // Mot de passe du WiFi

// ====== Infos du serveur  ======
const char* server = "51.38.41.157";         // Adresse IP ou nom de domaine du serveur
const int port = 1884;                       // Port du serveur securisé

// Objet client sécurisé (pour TLS) 
WiFiClientSecure client;                     //pour gérer  la connexion sécurisée TLS

// mise en place des etats du programme, 
enum State {
  WIFI_CONNECT,       // la connexion WiFi
  WIFI_WAIT,          // en attente de la connexion WiFi
  TLS_CONNECT,        // tentative de connexion au serveur TLS
  TLS_HANDSHAKE,      // vérification si le connexion au serveur est etablie
  SEND_MESSAGE,       // envoyer un message au serveur 
  READ_RESPONSE,      // lecture et affichage 
  IDLE                // gestion des actions repetetives si besoin
};
// reperesenter l'etat actuel du programme
State currentState = WIFI_CONNECT;

// je remplaçe le delay 
unsigned long lastActionTime = 0;    // Mémorise le moment de la dernière action
int wifiTries = 0;                   // Nombre de tentatives de connexion WiFi

bool declarationEnvoyee = false;     // Pour envoyer la déclaration une seule fois
bool temperatureEnvoyee = false;     // Pour envoyer la température après le broker

// l'initialisation
void setup() {
  Serial.begin(115200);            
  delay(500);                    
  client.setInsecure();             // Désactive la vérification du certificat
  client.setHandshakeTimeout(5);    // Timeout 
}

void loop() {
  unsigned long now = millis();     // Récupère le temps actuel en utlisant la fonction millis 

  // utilisation  du switch  pour representer les diffirents etats du programme selon son etat actuel  (chaque etat represente une etape)
  switch (currentState) {

    case WIFI_CONNECT:// connexion au  wifi
      Serial.printf("Connexion à %s...\n", ssid);
      WiFi.begin(ssid, password);          
      lastActionTime = now;                
      wifiTries = 0;                       
      currentState = WIFI_WAIT;            
      break;

    case WIFI_WAIT:
    //attente de la connexion  au wifi soit :
      //connexion au wifi reussie et il va afficher l '@ip
      if (WiFi.status() == WL_CONNECTED) {
        Serial.println("Connecté au WiFi !");
        Serial.print("Adresse IP : ");
        Serial.println(WiFi.localIP());    
        currentState = TLS_CONNECT;    // passer a l etat 3    
      } 
      // Sinon,il sera mis en attente 
      else if (now - lastActionTime >= 500) {
        lastActionTime = now;             
        Serial.print(".");                
        wifiTries++;    // Incrémentation de nombre de tentatives 
        // au bout de 30 tentaives , il va afficher un message d' echec                   
        if (wifiTries >= 30) {
          Serial.println("\n Échec de la connexion WiFi.");
          currentState = IDLE; // il va passer directement l'etat final
        }
      }
      break;
      // attente de la connexion au serveur tls
    case TLS_CONNECT:
      Serial.printf("\n Tentative de connexion à %s:%d\n", server, port);
      client.stop();                    
      lastActionTime = now;            
      declarationEnvoyee = false;
      temperatureEnvoyee = false;
      currentState = TLS_HANDSHAKE;  // Passe à l'état suivant      
      break;
        //connexion au serveur tls soit:
    case TLS_HANDSHAKE:
     // Si pas encore connecté
      if (!client.connected()) {
        // Tente une connexion TLS il repete 
        if (client.connect(server, port)) {
          Serial.printf(" Connexion TLS réussie à %s:%d\n", server, port);
          currentState = SEND_MESSAGE;    // Passe à l'envoi du message 
        } 
        // connexion echouée au serveur 
        else if (now - lastActionTime > 5000) {
          Serial.printf(" Échec de connexion TLS à %s:%d\n", server, port);
          currentState = IDLE;   //  il passe a l'etat final          
        }
      }
      break;
      //envoyer un message au serveur 
    case SEND_MESSAGE:
      client.println("#Lyza");            
      lastActionTime = now;              
      Serial.println(" Message envoyé au serveur.");
      currentState = READ_RESPONSE;  // Passe à l’étape de lecture et d 'affichage       
      break;
       // lecture et affichage du message envoyé
    case READ_RESPONSE:
    // s'il reponds
      if (client.available()) {
        String line = client.readStringUntil('\n');  // lecture 
        Serial.print(" Réponse : ");
        Serial.println(line);           // Affiche la réponse
        //declaration 
        if (!declarationEnvoyee) {
          String declaration = "!T/def TNozay";
          client.println(declaration);
          Serial.print(" Déclaration envoyée : ");
          Serial.println(declaration);
          declarationEnvoyee = true;
        }
        else if (!temperatureEnvoyee && line.startsWith("$broker")) {
          Serial.print(" Réponse serveur : ");
          Serial.println(line);  // Affiche directement la réponse 
          String temperature = "T [023.12]";
          client.println(temperature);
          //Affiche le message de température envoyé
          Serial.print(" Température envoyée : ");
          Serial.println(temperature);
          temperatureEnvoyee = true;// temperature bien envoyée
        }
      } 
      // Affiche qu’aucune réponse n’a été reçue après la tentative de souscription
      else if (now - lastActionTime > 3000) {
        Serial.println(" Pas de réponse à la souscription.");
        currentState = IDLE;
      }
      break;
       // Revient à l'étape initiale : la connexion WiFi
    case IDLE:
      Serial.println("\n repeter le processus"); 
      currentState = WIFI_CONNECT;     
      break;
  }
}


