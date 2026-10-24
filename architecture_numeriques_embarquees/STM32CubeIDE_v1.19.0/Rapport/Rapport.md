# Rapport de Projet — Maison Intelligente STM32

**Unité d'enseignement :** Architecture Numériques Embarquées  
**Membres du groupe :** _[À compléter]_  
**Date :** _[À compléter]_  
**Carte cible :** NUCLEO-F411RE (STM32F411RETx)

---

## Table des matières

1. [Introduction](#1--introduction)
2. [Architecture globale du système](#2--architecture-globale-du-système)
   1. [Brochage retenu](#21-brochage-retenu)
   2. [Arbre d'horloge et fréquences](#22-arbre-dhorloge-et-fréquences)
   3. [Organisation logicielle](#23-organisation-logicielle)
3. [Scénario 1 — Base temporelle pour l'échantillonnage](#3--scénario-1--base-temporelle-pour-léchantillonnage)
   1. [Configuration du Timer (TIM2)](#31-configuration-du-timer-tim2)
   2. [Validation expérimentale](#32-validation-expérimentale)
4. [Scénario 2 — Sécurité : HOME/AWAY et détection d'intrusion](#4--scénario-2--sécurité--homeaway-et-détection-dintrusion)
   1. [Bouton MODE et anti-rebond](#41-bouton-mode-et-anti-rebond)
   2. [Machine à états HOME / AWAY](#42-machine-à-états-home--away)
   3. [Capteur PIR et alarme intrusion](#43-capteur-pir-et-alarme-intrusion)
   4. [Gestion non bloquante des alarmes](#44-gestion-non-bloquante-des-alarmes)
5. [Scénario 3 — Détection de gaz](#5--scénario-3--détection-de-gaz)
   1. [Configuration de l'ADC et du DMA](#51-configuration-de-ladc-et-du-dma)
   2. [Caractérisation du MQ-2 et seuil](#52-caractérisation-du-mq-2-et-seuil)
   3. [Alarme gaz](#53-alarme-gaz)
6. [Scénario 4 — Supervision UART](#6--scénario-4--supervision-uart)
   1. [Configuration de l'USART2](#61-configuration-de-lusart2)
   2. [Redirection de printf](#62-redirection-de-printf)
   3. [Informations transmises](#63-informations-transmises)
7. [Scénario 5 — Gestion intelligente de la fenêtre](#7--scénario-5--gestion-intelligente-de-la-fenêtre)
   1. [Commande du servomoteur (TIM1 PWM)](#71-commande-du-servomoteur-tim1-pwm)
   2. [Bouton fenêtre et anti-rebond](#72-bouton-fenêtre-et-anti-rebond)
   3. [Détection de pluie (Steam Sensor via ADC)](#73-détection-de-pluie-steam-sensor-via-adc)
   4. [Logique de décision de la fenêtre](#74-logique-de-décision-de-la-fenêtre)
8. [Scénario 6 — Mesure de température (TMP102)](#8--scénario-6--mesure-de-température-tmp102)
   1. [Configuration de l'I²C (I2C3)](#81-configuration-de-li²c-i2c3)
   2. [Développement du driver TMP102](#82-développement-du-driver-tmp102)
   3. [Mode One-Shot et polling non bloquant](#83-mode-one-shot-et-polling-non-bloquant)
   4. [Conversion et affichage de la température](#84-conversion-et-affichage-de-la-température)
   5. [Gestion automatique de la fenêtre par la température](#85-gestion-automatique-de-la-fenêtre-par-la-température)
9. [Scénario 7 — Intégration et affichage LCD](#9--scénario-7--intégration-et-affichage-lcd)
   1. [Intégration de la bibliothèque LCD I²C](#91-intégration-de-la-bibliothèque-lcd-i²c)
   2. [Stratégie d'affichage sur écran 16×2](#92-stratégie-daffichage-sur-écran-16×2)
   3. [Cohabitation LCD et TMP102 sur le même bus I²C](#93-cohabitation-lcd-et-tmp102-sur-le-même-bus-i²c)
10. [Bibliothèque Bande LED RGB (WS2812B)](#10--bibliothèque-bande-led-rgb-ws2812b)
    1. [Principe de communication (protocole one-wire)](#101-principe-de-communication-protocole-one-wire)
    2. [Implémentation PWM + DMA (TIM4)](#102-implémentation-pwm--dma-tim4)
    3. [Tableau de bord LED](#103-tableau-de-bord-led)
11. [Gestion globale des alarmes](#11--gestion-globale-des-alarmes)
    1. [Clignotement non bloquant par compteur partagé](#111-clignotement-non-bloquant-par-compteur-partagé)
    2. [Gestion du buzzer avec fréquences distinctes](#112-gestion-du-buzzer-avec-fréquences-distinctes)
    3. [Cas simultané : alternance gaz / intrusion](#113-cas-simultané--alternance-gaz--intrusion)
12. [Difficultés rencontrées et solutions](#12--difficultés-rencontrées-et-solutions)
13. [Conclusion](#13--conclusion)

---

## 1 — Introduction

> _Présenter brièvement le contexte du projet (maquette Keyestudio Smart Home, carte NUCLEO-F411RE), l'objectif global (application embarquée unique et cohérente intégrant tous les scénarios), et les outils utilisés (STM32CubeMX, STM32CubeIDE, HAL)._

---

## 2 — Architecture globale du système

### 2.1 Brochage retenu

#### Contraintes de brochage

On a voulu rester sur l'**Arduino Header** de la Nucleo pour simplifier le câblage avec la breadboard notamment. Si on reprend le pinout fourni sur BlackBoard : 

![Fonctions alternatives disponibles sur l'Arduino Header de la NUCLEO-F411RE](media/Arduino_Header_with legend.png)

A partir du scenario 6, il a fallu reorganiser le brochage arbitraire qu'on faisait au début pour tout faire rentrer dur le header Arduino.
ON a donc listé tous les composants et leur fonctions pour leur assigner des broches du plus contraignant au moins contraignant.

---

**Priorité 1 — Broches fixées par le matériel**

Les broches suivantes sont assignées d'emblée:

- **PA2 & PA3** → USART2 TX/RX, reliés physiquement au ST-LINK sur la Nucleo pour la liaison série USB.
- **PA5 (D13)** → LED built-in, inutilisée pour pouvoir déporter les LEDs (avant d'implémenter les NeoPixels).
- **PB10 (D6)** → LED bleue que nous avons soudé sur le shield.

---

**Priorité 2 — I²C : SCL et SDA**

Seules 2 broches supportent les fonctions I2Cx_SCL & I2Cx_SDA donc on les a réservée aussi directement :

Le module **I2C3** est disponible avec :
- SCL sur **PA8 (D7)**
- SDA sur **PB4 (D5)**

---

**Priorité 3 — Timers**

On avait besoin de 4 timers :
- PWM du servomoteur
- PWM du buzzer passif
- PWM de la bande de 4 LEDs NeoPixel
- Timer pour l'échantillonnage

On a que 4 timers dispos sur le header Arduino.
Or pour l'échantillonnage on voulait que l'ADC soit déclenché par un TGO event du timer d'echantillonnage, or il y a que les timers 2 et 3 qui permettent ça.
On a donc choisit arbitrairement le timer 2 pour l'échantillonnage.
Pour le reste on a choisit arbitrairement quel composant allait sur quel timer, sachant que APB1 et APB2 sont tous les deux à 100Mhz donc pas de différence sur ce point.

Résultat :
- Timer 1 → Servomoteur sur le channel 3 : PA9 (D8)
- Timer 2 → Echantillonnage
- Timer 3 → Buzzer sur le channel 2 : PB5 (D4)
- Timer 4 → LEDs NeoPixel sur le channel 1 : PA6 (D11)

---

**Priorité 4 — Voies analogiques**

Deux mesures analogiques sont nécessaires (gaz MQ-2 et capteur de pluie). 
L'ADC1 est le seul ADC de la carte. mais on a plusieurs entrées sur les broches analogiques notemment qui sont traditionnellement reservées aux mesures analogiques. On arbitrairement chois parmi les AX restantes : 

- **A2 (PA4)** → ADC1 canal 4 → capteur de gaz MQ-2
- **A3 (PB0)** → ADC1 canal 8 → capteur de pluie

---

**Priorité 5 — Broches d'interruption**

On avait besoin de 3 interruptions externes (EXTI) :
- **Bouton HOME** (bascule mode HOME/AWAY)
- **Bouton FENÊTRE** (commande manuelle de la fenêtre)
- **Capteur PIR** (détection de mouvement)

La contrainte importante est que deux broches de **même numéro** sur des ports différents partagent la même ligne EXTI. Il fallait choisir des broches qui aient un numéro différent. On a choisi :

- **PA6 (D12)** → EXTI 6   → Bouton HOME
- **PA10 (D2)** → EXTI 10  → Bouton FENÊTRE
- **PC7 (D9)**  → EXTI 7   → PIR

---

**Priorité 6 — Sorties GPIO**

On avait besoin de 3 sorties GPIO pour les LEDs indicatrices. On a choisi arbitrairement parmi les broches restantes :

- **PB3 (D3)** → LED jaune du shield : Alarme intrusion
- **PA7 (D11)** → LED RGB couleur rouge du shield : Alarme gaz
- **PB10 (D6)** → LED bleue (imposé par la soudure) : cycle capteurs

PS : C'est seulement à la fin que nous avons décidé d'ajouter les 4 LEDs NeoPixels WS2812B en bonus. Mais on l'avait déjà prise en compte au moment du refacto des pins. Finalement on a gardé les 2 systèmes de LED ensemble.

---

#### Tableau de brochage final

| Composant | Label CubeMX | Connecteur Arduino | Broche STM32 | Périphérique / Mode | Pull | Tension |
|---|---|---|---|---|---|---|
| Bouton fenêtre | BTN_FENTR | D2 | PA10 | EXTI10 – Falling Edge | PULLUP | 3V3 |
| LED alarme intrusion | LED_JAUNE | D3 | PB3 | GPIO Output | — | 3V3 |
| Buzzer passif | BUZZER | D4 | PB5 | TIM3 CH2 – PWM | — | 3V3 |
| TMP102 + LCD (SDA) | I2C_SDA | D5 | PB4 | I2C3 SDA | — | 5V |
| LED cycle capteurs | LED_BLEUE | D6 | PB10 | GPIO Output | — | 3V3 |
| TMP102 + LCD (SCL) | I2C_SCL | D7 | PA8 | I2C3 SCL | — | 5V |
| Servomoteur fenêtre | SERVO | D8 | PA9 | TIM1 CH2 – PWM | — | 5V |
| Capteur PIR | PIR_MVMT | D9 | PC7 | EXTI7 – Rising Edge | PULLDOWN | 3V3 |
| Bande LED NeoPixel | RGB_BANDE | D10 | PB6 | TIM4 CH1 – PWM + DMA | — | 5V |
| LED alarme gaz | RGB_ROUGE | D11 | PA7 | GPIO Output | — | 3V3 |
| Bouton HOME/AWAY | BTN_HOME | D12 | PA6 | EXTI6 – Falling Edge | PULLUP | 3V3 |
| Capteur gaz MQ-2 | GAZ | A2 | PA4 | ADC1 CH4 | — | 5V |
| Capteur de pluie | PLUIE | A3 | PB0 | ADC1 CH8 | — | 5V |
| Liaison série USB | SERIAL_TX | — | PA2 | USART2 TX (ST-LINK) | — | 3V3 |
| Liaison série USB | SERIAL_RX | — | PA3 | USART2 RX (ST-LINK) | — | 3V3 |

### 2.2 Arbre d'horloge et fréquences

> _Décrire la configuration de l'horloge système : HSE 8 MHz, PLL configuré (PLLM=4, PLLN=100, PLLP=2) pour obtenir SYSCLK = 100 MHz. Indiquer les fréquences dérivées : AHB = 100 MHz, APB1 = 50 MHz (timers APB1 = 100 MHz), APB2 = 100 MHz. Expliquer pourquoi ces fréquences sont importantes pour le calcul des prescalers/périodes des timers._

### 2.3 Organisation logicielle

> _Décrire l'architecture logicielle du projet : une boucle principale (`while(1)`) pilotée par des flags positionnés dans des callbacks d'interruption (Timer, EXTI, ADC/DMA). Expliquer le principe de fonctionnement non bloquant : aucune boucle d'attente active dans le `main`, les ISR sont courtes (positionnent un flag), et le traitement se fait dans la boucle principale. Mentionner l'organisation en bibliothèques séparées (TMP102, Bande LED, LCD fourni)._

---

## 3 — Scénario 1 — Base temporelle pour l'échantillonnage

### 3.1 Configuration du Timer (TIM2)

> _Justifier le choix de TIM2 pour la base temporelle d'échantillonnage. Détailler le calcul : horloge timer = 100 MHz, Prescaler = 10000-1 → fréquence après prescaler = 10 kHz, Period = 20000-1 → période de 2 s (0.5 Hz). Expliquer que TIM2 est également configuré en Master avec TRGO = Update pour déclencher automatiquement les conversions ADC._

### 3.2 Validation expérimentale

> _Décrire comment tu as validé le timer : toggling de la LED bleue (PA5 ou PB10) à chaque interruption TIM2 et vérification à l'oscilloscope de la période de 2 s._

---

## 4 — Scénario 2 — Sécurité : HOME/AWAY et détection d'intrusion

### 4.1 Bouton MODE et anti-rebond

> _Expliquer le choix d'utiliser le bouton sur PA6 en EXTI sur front descendant (falling edge) avec pull-up interne. Justifier le mécanisme d'anti-rebond implémenté : à l'appui, un flag `debouncing_bouton_home` est levé et le compteur de TIM5 est remis à zéro. TIM5 est configuré à 20 Hz (Prescaler=10000-1, Period=500-1 → 50 ms). Au prochain tick de TIM5, `debounce_filter()` vérifie que le bouton est bien relâché (état stable) avant de valider l'appui. Expliquer pourquoi cette méthode est plus fiable qu'un simple délai bloquant._

### 4.2 Machine à états HOME / AWAY

> _Décrire la machine à états dans la boucle principale : état `HOME` (éclairage/fenêtre actifs, pas d'alarme intrusion) et état `AWAY` (fenêtre forcée fermée, bouton fenêtre ignoré, détection PIR active). Expliquer les transitions : bouton MODE bascule HOME→AWAY (ferme la fenêtre) ou AWAY→HOME (désactive l'alarme intrusion). Justifier que le changement de mode est signalé sur l'écran LCD et via UART._

### 4.3 Capteur PIR et alarme intrusion

> _Décrire la configuration du PIR sur PC7 en EXTI Rising Edge avec pull-down. Expliquer le comportement : en mode AWAY, le front montant positionne `presence_detectee = true`, ce qui déclenche l'alarme intrusion dans la boucle principale. En mode HOME, l'interruption PIR est ignorée. Justifier que le traitement est minimal dans l'ISR (un simple flag)._

### 4.4 Gestion non bloquante des alarmes

> _Renvoyer vers la section 11 pour le détail de l'implémentation non bloquante du clignotement LED + buzzer._

---

## 5 — Scénario 3 — Détection de gaz

### 5.1 Configuration de l'ADC et du DMA

> _Expliquer la configuration de l'ADC1 en mode Scan avec 2 canaux (CH4 pour le gaz, CH8 pour la pluie). Justifier l'utilisation du DMA en mode circulaire (DMA2 Stream0) pour le transfert automatique des résultats dans le tableau `adc_valeurs[2]`. Expliquer le déclenchement externe par TIM2 TRGO (Event Update) : à chaque période de 2 s, une séquence de conversion est automatiquement lancée sans intervention logicielle. Le callback `HAL_ADC_ConvCpltCallback` positionne un flag pour le traitement dans la boucle principale._

### 5.2 Caractérisation du MQ-2 et seuil

> _Décrire les mesures expérimentales réalisées pour caractériser le capteur MQ-2 : valeur au repos (air ambiant) vs. valeur en présence de gaz (briquet/allumette). Justifier le seuil `SEUIL_GAZ = 2500` retenu sur une plage ADC de 0–4095 (12 bits). Mentionner le temps de chauffe du capteur si observé._

### 5.3 Alarme gaz

> _Expliquer que la surveillance du gaz est active en permanence (indépendante du mode HOME/AWAY), conformément au cahier des charges. Quand `gaz_valeur >= SEUIL_GAZ`, le flag `alarme_gaz` est activé, ce qui déclenche le clignotement de la LED rouge (RGB_ROUGE sur PA7) et le buzzer à 440 Hz. Un message « Gaz detecte ! » est envoyé sur UART et affiché sur le LCD._

---

## 6 — Scénario 4 — Supervision UART

### 6.1 Configuration de l'USART2

> _Expliquer que l'USART2 (PA2 TX, PA3 RX) est la liaison série reliée au ST-LINK sur la Nucleo, permettant la communication avec un terminal PC via USB. Mentionner la vitesse par défaut (115200 baud)._

### 6.2 Redirection de printf

> _Justifier le choix de rediriger `printf` via `__io_putchar()` qui appelle `HAL_UART_Transmit()`. Mentionner l'avantage : utilisation facile de `printf` et `snprintf` dans tout le code pour le debug et la supervision._

### 6.3 Informations transmises

> _Lister les informations transmises via UART : message de bienvenue au démarrage (seuils configurés, fréquences), valeurs des capteurs à chaque cycle (GAZ, PLUIE, TEMP), événements (changement de mode, ouverture/fermeture fenêtre, alarmes intrusion/gaz)._

---

## 7 — Scénario 5 — Gestion intelligente de la fenêtre

### 7.1 Commande du servomoteur (TIM1 PWM)

> _Expliquer la configuration de TIM1 CH2 en PWM pour le servo (PA9). Détailler le calcul : horloge APB2 Timer = 100 MHz, Prescaler = 40-1 → fréquence timer = 2.5 MHz, Period = 50000-1 → fréquence PWM = 50 Hz (période de 20 ms, standard servo). Décrire la fonction `angle_servo(int angle)` qui convertit un angle (0°–180°) en nombre de ticks de comparaison : impulsion de 0.5 ms (0°) à 2.5 ms (180°). Mentionner la limitation matérielle à 120° max pour éviter d'endommager le montage._

### 7.2 Bouton fenêtre et anti-rebond

> _Expliquer que le bouton fenêtre (PA10, EXTI Falling Edge, Pull-up) utilise le même mécanisme d'anti-rebond que le bouton MODE via TIM5. Justifier le choix du front descendant + pull-up (le bouton est actif bas). En mode HOME et sans pluie, chaque appui bascule la fenêtre entre OUVERTE et FERMEE. En mode AWAY ou en cas de pluie, l'appui est ignoré._

### 7.3 Détection de pluie (Steam Sensor via ADC)

> _Décrire le Steam Sensor connecté sur PB0 (ADC1 CH8). Expliquer la caractérisation expérimentale et justifier le seuil `SEUIL_PLUIE = 4000`. Quand la pluie est détectée, la fenêtre est forcée fermée et le bouton fenêtre est ignoré tant que la condition persiste._

### 7.4 Logique de décision de la fenêtre

> _Résumer les règles de priorité de la fenêtre : (1) mode AWAY → fermée, (2) pluie détectée → fermée, (3) commande manuelle (bouton) si HOME et pas de pluie, (4) commande automatique par température (voir scénario 6). Expliquer que `commande_fenetre()` n'envoie le signal PWM au servo que lors d'un changement d'état (variable `etat_precedent`) pour éviter des commandes inutiles._

---

## 8 — Scénario 6 — Mesure de température (TMP102)

### 8.1 Configuration de l'I²C (I2C3)

> _Justifier le choix de I2C3 (SCL = PA8, SDA = PB4) pour communiquer avec le TMP102 et le LCD. Mentionner l'adresse I²C du TMP102 : 0x48 (broche ADD0 à GND), décalée à gauche de 1 bit pour le HAL (0x90). Indiquer le mode standard (100 kHz ou 400 kHz selon la config)._

### 8.2 Développement du driver TMP102

> _Décrire l'architecture du driver développé sans bibliothèque prête à l'emploi. Expliquer les structures de données : `TMP102_t` (état complet du capteur), `TMP102_Config_t` (champs du registre de configuration), `TMP102_FieldMap_t` (mapping bit-à-bit des champs). Détailler le mécanisme générique de sérialisation/désérialisation du registre de configuration via `TMP102_SyncConfigBuffer()` : un tableau de `FieldMap` décrit pour chaque champ son byte, sa position et sa longueur, permettant l'encodage (structure → buffer) et le décodage (buffer → structure) en une seule fonction bidirectionnelle. Justifier ce choix d'architecture (maintenabilité, lisibilité, réduction des erreurs de masquage manuel)._

### 8.3 Mode One-Shot et polling non bloquant

> _Justifier le choix du mode Shutdown + One-Shot plutôt que le mode continu : économie d'énergie et synchronisation avec la base de temps de 2 s. Expliquer la séquence : à chaque tick TIM2, `TMP102_start_measure()` écrit le bit OS=1 dans le registre de config. Puis, dans la boucle principale, `TMP102_is_measure_ready()` lit le registre de config et vérifie si OS est repassé à 1 (conversion terminée). Ce polling est non bloquant car il ne fait qu'un appel I²C par tour de boucle._

### 8.4 Conversion et affichage de la température

> _Décrire les fonctions `TMP102_register_to_celsius()` et `TMP102_celsius_to_register()` : concaténation des 2 bytes, décalage selon le mode (12 ou 13 bits), multiplication par la résolution (0.0625 °C/LSB). Expliquer `TMP102_temperature_to_string()` : conversion float→string sans `%f` (décomposition partie entière/décimale avec arrondi) pour contourner les limitations de `snprintf` en embarqué._

### 8.5 Gestion automatique de la fenêtre par la température

> _Décrire le mécanisme optionnel implémenté : trois états de température (FROID, TEMPEREE, CHAUD) avec hystérésis (seuil chaud = 26 °C, seuil froid = 24 °C). Quand la température passe en CHAUD (≥26 °C) et qu'on est en HOME sans pluie, la fenêtre s'ouvre pour ventiler. Quand la température passe en FROID (≤24 °C), la fenêtre se ferme. L'hystérésis évite les oscillations autour du seuil. Justifier les valeurs retenues._

---

## 9 — Scénario 7 — Intégration et affichage LCD

### 9.1 Intégration de la bibliothèque LCD I²C

> _Décrire l'intégration de la bibliothèque LCD fournie. Mentionner l'adresse I²C du LCD : 0x27 (décalée 0x4E). Expliquer la fonction `lcd_init()` qui prend en paramètre le bus I²C (I2C3 partagé avec le TMP102) et réalise la séquence d'initialisation en mode 4 bits._

### 9.2 Stratégie d'affichage sur écran 16×2

> _Justifier l'organisation de l'affichage sur les 2 lignes de 16 caractères :_
> - _Ligne 1 : `HOME  18,38°C` — mode courant (4 car.) + température (8 car.)_
> - _Ligne 2 : `Hu 1234  Ga 1234` — valeur pluie et valeur gaz_
>
> _Expliquer que l'affichage du mode n'est mis à jour qu'au changement, et que les valeurs des capteurs sont mises à jour à chaque cycle de 2 s (après la dernière mesure disponible : la température). Justifier ce découpage pour maximiser l'information affichée dans l'espace contraint. Mentionner le message de bienvenue au démarrage._

### 9.3 Cohabitation LCD et TMP102 sur le même bus I²C

> _Expliquer que le LCD (0x27) et le TMP102 (0x48) ont des adresses I²C différentes et partagent le bus I2C3 sans conflit. Mentionner les précautions éventuelles (pas d'accès I²C concurrent depuis les ISR)._

---

## 10 — Bibliothèque Bande LED RGB (WS2812B)

### 10.1 Principe de communication (protocole one-wire)

> _Décrire le protocole des LED WS2812B : signal one-wire à 800 kHz, chaque bit est codé par la durée de l'impulsion haute (400 ns pour un 0, 800 ns pour un 1) sur une période fixe. L'ordre des couleurs dans la trame est GRB. Un délai de reset (≥ 50 µs) termine la trame._

### 10.2 Implémentation PWM + DMA (TIM4)

> _Justifier le choix de TIM4 CH1 (PB6) en PWM + DMA pour générer le signal : chaque bit correspond à un cycle PWM dont le duty cycle est ajusté par une valeur dans le buffer DMA. Le timer est cadencé à 100 MHz, Period = 125-1 → fréquence PWM = 800 kHz. Le DMA (DMA1 Stream0, Memory to Peripheral) transfère automatiquement les valeurs de duty cycle pour chaque bit de chaque LED. Détailler la construction du buffer DMA dans `BandeLED_show()` : pour chaque LED, pour chaque composante (GRB), pour chaque bit (MSB first), la valeur de comparaison est soit `pulse_bit_0` soit `pulse_bit_1`. Les cycles de reset (duty = 0) sont ajoutés à la fin._

### 10.3 Tableau de bord LED

> _Expliquer l'utilisation de la bande de 4 LEDs RGB comme tableau de bord visuel :_
> - _LED haut gauche (violet) : clignote à chaque cycle capteurs (témoin d'activité)_
> - _LED haut droite (rouge) : état de l'alarme gaz (clignotante si alerte)_
> - _LED bas droite (orange) : état de l'alarme intrusion (clignotante si alerte)_
> - _LED bas gauche : indicateur de température (bleu = froid, vert = tempéré, orange = chaud)_
>
> _Justifier le choix de la luminosité réduite (`LUMINOSITE_BASE = 10/255`) pour éviter l'éblouissement._

---

## 11 — Gestion globale des alarmes

### 11.1 Clignotement non bloquant par compteur partagé

> _Expliquer le mécanisme de clignotement non bloquant : un compteur `blink_counter_alarm` est incrémenté à chaque tick de TIM5 (20 Hz). Pour chaque alarme, une demi-période de clignotement est calculée en nombre de ticks (ex : alarme gaz à 10 Hz → demi-période = 1 tick ; alarme intrusion à 2 Hz → demi-période = 5 ticks). L'état de la LED est inversé à chaque fin de demi-période. Justifier que ce mécanisme est entièrement non bloquant._

### 11.2 Gestion du buzzer avec fréquences distinctes

> _Expliquer que le buzzer passif (PB5, TIM3 CH2) est piloté en PWM avec un duty cycle de 50 %. La fréquence du buzzer est ajustée dynamiquement via `frequence_buzzer()` qui modifie l'auto-reload et le compare de TIM3. L'alarme gaz sonne à 440 Hz (La4), l'alarme intrusion à 384 Hz. Le buzzer est synchronisé avec l'état de la LED correspondante (il sonne quand la LED est allumée)._

### 11.3 Cas simultané : alternance gaz / intrusion

> _Décrire la stratégie quand les deux alarmes sont actives simultanément : le buzzer alterne entre les deux fréquences (intrusion puis gaz) par demi-périodes, tandis que chaque LED clignote indépendamment à sa propre fréquence. Justifier ce choix pour permettre d'identifier les deux alarmes même quand elles sont concomitantes._

---

## 12 — Difficultés rencontrées et solutions

> _Lister les principaux problèmes rencontrés durant le projet et les solutions apportées. Exemples possibles :_
> - _Rebonds des boutons poussoirs → implémentation du filtre anti-rebond par timer_
> - _Protocole WS2812B exigeant en timing → utilisation du DMA + PWM pour garantir la précision_
> - _Cohabitation I²C entre LCD et TMP102 → vérification des adresses, pas d'accès concurrent_
> - _Absence de `%f` fiable dans `snprintf` embarqué → écriture d'une fonction de conversion maison_
> - _Limitation mécanique du servo → angle max borné à 120°_
> - _Temps de chauffe du MQ-2 et stabilisation des mesures_

---

## 13 — Conclusion

> _Synthétiser les résultats obtenus : tous les scénarios sont intégrés dans une application unique, la boucle principale gère de manière non bloquante l'ensemble des capteurs/actionneurs, les bibliothèques TMP102 et Bande LED ont été développées en propre. Mentionner les éventuelles améliorations possibles (AHT20 pour humidité, ajout d'un mode alerte LCD plus visible, gestion de priorités d'interruption, etc.)._
