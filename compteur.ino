
#include <Wire.h> 
#include <LiquidCrystal_I2C.h>
#include <EEPROM.h>

#define CAPTEUR_TOUR 2  // Capteur de tours (interruption)
#define CAPTEUR_EJECTION 3 // Capteur d'éjection
#define LED_VERTE 4
#define LED_ORANGE 5
#define LED_ROUGE 6

#define BTN_MENU 7
#define BTN_UP 8
#define BTN_DOWN 9
#define BTN_LEFT 10
#define BTN_RIGHT 11

LiquidCrystal_I2C lcd(0x27, 16, 2);
volatile int toursRestants;
volatile int bottes = 0;
volatile int toursParBotte = 35;

int dernierEtatCapteurTour = HIGH;  
int dernierEtatCapteurEjection = HIGH;
int dernierEtatBoutonMenu = HIGH;

unsigned long dernierTourDetecte = 0;
unsigned long dernierEjectionDetecte = 0;
unsigned long dernierTempsMenu = 0;
const unsigned long delaiInactivite = 5000;

bool modeMenu = false;
int menuSelection = 1;

void afficherTexte(int x, int y, const char* texte, int valeur = -1);
void setup() {
    Serial.begin(9600);

    pinMode(CAPTEUR_TOUR, INPUT_PULLUP);
    pinMode(CAPTEUR_EJECTION, INPUT_PULLUP);
    pinMode(LED_VERTE, OUTPUT);
    pinMode(LED_ORANGE, OUTPUT);
    pinMode(LED_ROUGE, OUTPUT);
    pinMode(BTN_MENU, INPUT_PULLUP);
    pinMode(BTN_UP, INPUT_PULLUP);
    pinMode(BTN_DOWN, INPUT_PULLUP);
    pinMode(BTN_LEFT, INPUT_PULLUP);
    pinMode(BTN_RIGHT, INPUT_PULLUP);

    lcd.init();
    lcd.backlight();

    long vcc = lireVcc(); // Lire la tension de la batterie
    Serial.print("🔋 Tension Vcc: "); Serial.print(vcc); Serial.println(" mV");

    // Si la batterie est faible (par exemple, en dessous de 4700 mV)
    if (vcc < 4600) {
        // Afficher un message de batterie faible sur le LCD
        lcd.clear();
        afficherTexte(0, 0, "Batterie faible");
        afficherTexte(0, 1, "Changer la pile");

        // Empêcher toute autre interaction tant que la pile n'est pas changée
        while (true) {
            // Rester ici et ne rien faire tant que la batterie est faible
            delay(1000);  // Attendre pour afficher continuellement le message
        }
    }

    EEPROM.get(0, bottes);
    EEPROM.get(4, toursParBotte);
    EEPROM.get(8, toursRestants);

    if (toursRestants > toursParBotte || toursRestants < 0) {
        toursRestants = toursParBotte;
    }

    dernierEtatCapteurTour = digitalRead(CAPTEUR_TOUR);
    dernierEtatCapteurEjection = digitalRead(CAPTEUR_EJECTION);

    Serial.println("📢 Démarrage du système...");
    afficherLCD();
}

void loop() {
  int etatBoutonMenu = digitalRead(BTN_MENU);
    if (etatBoutonMenu == LOW && dernierEtatBoutonMenu == HIGH) {
        modeMenu = !modeMenu;
        Serial.print("Mode Menu: "); Serial.println(modeMenu ? "ON" : "OFF");
        afficherLCD();
        dernierTempsMenu = millis();
        delay(300);
    }
    dernierEtatBoutonMenu = etatBoutonMenu;
  if (modeMenu) {
        gererMenu();
    } else {
        gererFeuTricolore();
        surveillerCapteurs();
        if (digitalRead(BTN_MENU) == LOW) {
            modeMenu = true;
            afficherLCD();
            delay(300);
        }
    }
    delay(10);
}

long lireVcc() {
    ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1); // lire la référence interne 1.1V
    delay(2); // Laisser le temps au signal de se stabiliser
    ADCSRA |= _BV(ADSC); // Lancer la conversion
    while (bit_is_set(ADCSRA, ADSC)); // Attendre la fin
    uint16_t resultat = ADC;
    long vcc = 1125300L / resultat; // 1.1*1023*1000 en mV
    return vcc; // Exemple : 4980 pour 4.98V
}

void surveillerCapteurs() {
    gererFeuTricolore();

    int etatCapteurTour = digitalRead(CAPTEUR_TOUR);
    int etatCapteurEjection = digitalRead(CAPTEUR_EJECTION);

    //Serial.print("État CAPTEUR_TOUR: "); Serial.print(etatCapteurTour);
    //Serial.print(" | État CAPTEUR_EJECTION: "); Serial.println(etatCapteurEjection);

    // Détection des tours
    if (etatCapteurTour == LOW && dernierEtatCapteurTour == HIGH && millis() - dernierTourDetecte > 100) {
        decrementerTour();
        dernierTourDetecte = millis();
    }

    // Détection de l’éjection (capteur passe LOW)
    if (etatCapteurEjection == LOW && dernierEtatCapteurEjection == HIGH && millis() - dernierEjectionDetecte > 500) {
        incrementerBotte();
        dernierEjectionDetecte = millis();
    }

    dernierEtatCapteurTour = etatCapteurTour;
    dernierEtatCapteurEjection = etatCapteurEjection;

}

void decrementerTour() {
    Serial.println("🚀 Tour détecté !");
    if (toursRestants > 0) {
        toursRestants--;

        int sauvegardeTours;
        EEPROM.get(8, sauvegardeTours);
        if (toursRestants != sauvegardeTours) {
            EEPROM.put(8, toursRestants);
        }

        Serial.print("📉 Tours restants : "); Serial.println(toursRestants);
        afficherLCD();
    } else {
        Serial.println("⚠️ Plus de tours restants !");
    }
}

void incrementerBotte() {
    Serial.println("🆕 Détection d’éjection !");

    // Vérifier que la botte est bien terminée
    if (toursRestants <= 0) {
        bottes++;
        toursRestants = toursParBotte;

        EEPROM.put(0, bottes);
        EEPROM.put(8, toursRestants);

        Serial.println("✅ Nouvelle botte comptabilisée !");
        afficherLCD();
    } else {
        Serial.println("⚠️ Tours restants non terminés, éjection ignorée.");
    }
}

// ✅ Fonction générique pour afficher du texte à une position donnée
void afficherTexte(int x, int y, const char* texte, int valeur = -1) {
    lcd.setCursor(x, y);
    
    for (int i = 0; texte[i] != '\0'; i++) {
        lcd.print(texte[i]);
        delay(10);
    }

    if (valeur != -1) {
        char buffer[6];
        sprintf(buffer, "%d", valeur);

        for (int i = 0; buffer[i] != '\0'; i++) {
            lcd.print(buffer[i]);
            delay(10);
        }

        lcd.print(" ");
    }
}

// ✅ Fonction principale d'affichage
void afficherLCD() {
    lcd.clear();
    Serial.println("📟 Mise à jour LCD...");
    afficherTexte(0, 0, "Tours: ", toursRestants);
    afficherTexte(0, 1, "Bottes: ", bottes);
    Serial.println("✅ Affichage terminé !");
}

// ✅ Gestion du feu tricolore
void gererFeuTricolore() {
    if (toursRestants > 5) {
        digitalWrite(LED_VERTE, HIGH);
        digitalWrite(LED_ORANGE, LOW);
        digitalWrite(LED_ROUGE, LOW);
    } else if (toursRestants == 5) {
        digitalWrite(LED_VERTE, LOW);
        digitalWrite(LED_ORANGE, HIGH);
        digitalWrite(LED_ROUGE, LOW);
    } else if (toursRestants == 0) {
        digitalWrite(LED_VERTE, LOW);
        digitalWrite(LED_ORANGE, LOW);
        digitalWrite(LED_ROUGE, HIGH);
    }
}

void gererMenu() {
    lcd.clear();
    afficherTexte(0, 0, "Reglage:");
    afficherTexte(0, 1, menuSelection == 1 ? "1: Modif tours" : "2: RAZ bottes");

    while (modeMenu) {
        if (millis() - dernierTempsMenu > delaiInactivite) {
            modeMenu = false;
            afficherLCD();
            return;
        }

        if (digitalRead(BTN_UP) == LOW) {
            menuSelection = 1;
            afficherTexte(0, 1, "1: Modif tours");
            dernierTempsMenu = millis();
            delay(300);
        }
        if (digitalRead(BTN_DOWN) == LOW) {
            menuSelection = 2;
            afficherTexte(0, 1, "2: RAZ bottes");
            dernierTempsMenu = millis();
            delay(300);
        }
        if (digitalRead(BTN_MENU) == LOW) {
            if (menuSelection == 1) {
                modifierTours();
            } else {
                confirmerRAZBottes();
            }
            modeMenu = false;
            dernierTempsMenu = millis();
            delay(300);
        }
    }
}

void afficherMenuPrincipal() {
    lcd.clear();
    afficherTexte(0, 0, "1: Modif tours");
    afficherTexte(0, 1, "2: RAZ bottes");
}

void modifierTours() {
    lcd.clear();
    afficherTexte(0, 0, "Changer tours:");
    afficherTexte(0, 1, "", toursParBotte);
    int nouvelleValeur = toursParBotte;

    while (true) {
        if (digitalRead(BTN_UP) == LOW) { 
            nouvelleValeur++; 
            afficherTexte(0, 1, "", nouvelleValeur);
            delay(300); 
        }
        if (digitalRead(BTN_DOWN) == LOW) { 
            nouvelleValeur--; 
            afficherTexte(0, 1, "", nouvelleValeur);
            delay(300); 
        }
        if (digitalRead(BTN_MENU) == LOW) { 
            toursParBotte = nouvelleValeur;
            EEPROM.put(4, toursParBotte); 
            
            // 🔁 Met à jour toursRestants aussi
            toursRestants = toursParBotte;
            EEPROM.put(8, toursRestants); 

            afficherLCD(); 
            modeMenu = false; 
            return; 
        }
    }
}

void confirmerRAZBottes() {
    int choix = 1; // 1: Non, 2: Oui

    lcd.clear();
    afficherTexte(0, 0, "RAZ Bottes?");
    afficherTexte(0, 1, "1: Non");

    while (true) {
        if (digitalRead(BTN_UP) == LOW || digitalRead(BTN_DOWN) == LOW) {
            choix = (choix == 1) ? 2 : 1;
            lcd.setCursor(0, 1);
            lcd.print("                "); // Efface la ligne
            lcd.setCursor(0, 1);
            if (choix == 1) {
                lcd.clear();
                afficherTexte(0, 0, "RAZ Bottes?");
                afficherTexte(0, 1, "1: Non");
            } else {
                lcd.clear();
                afficherTexte(0, 0, "RAZ Bottes?");
                afficherTexte(0, 1, "2: Oui");
            }
            delay(300);
        }

        if (digitalRead(BTN_MENU) == LOW) {
            if (choix == 2) {
                bottes = 0;
                EEPROM.put(0, bottes);
            }
            afficherLCD();
            modeMenu = false;
            delay(300);
            return;
        }
    }
}