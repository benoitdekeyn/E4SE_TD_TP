# Énoncé du Projet — Maison Intelligente STM32

---

## 1 — Présentation et objectifs

### Votre mission

Votre mission est de concevoir et intégrer le logiciel embarqué d'une maison intelligente à partir de la maquette Keyestudio Smart Home et d'une carte à microcontrôleur STM32F4.

Le STM32 devra acquérir les données des capteurs, traiter les événements, appliquer les règles de décision et piloter les actionneurs pour répondre aux scénarios du cahier des charges.

Vous programmerez l'application en langage C avec les bibliothèques STM32 HAL. La configuration des périphériques sera réalisée avec STM32CubeMX, puis le développement avec STM32CubeIDE.

> **Objectif final :** Faire fonctionner l'ensemble des scénarios au sein d'une application unique et cohérente, dans laquelle les fonctions interagissent et les décisions restent compatibles lorsqu'elles sont sollicitées simultanément.

---

### Cahier des Charges

Le système sera une application unique intégrant les fonctions suivantes :

- Acquérir les mesures à intervalles réguliers, toutes les 2 à 5 secondes (luminosité, température, humidité, gaz et pluie)
- Détecter les mouvements et prendre en compte les commandes de l'utilisateur.
- Commander l'éclairage, la fenêtre motorisée et les alarmes visuelles et sonores.
- Afficher les mesures et l'état du système sur le LCD, et transmettre les informations de supervision au PC.
- Définir des règles de décision cohérentes lorsque plusieurs événements ou commandes surviennent en même temps.

> **Objectif d'intégration :** Un seul système piloté par un seul programme faisant coopérer l'ensemble des scénarios.

Le système distingue les situations **HOME** (Habitation occupée) et **AWAY** (Habitation inoccupée) :

- En **HOME**, l'éclairage et l'ouverture de la fenêtre peuvent répondre aux mesures et aux commandes de l'utilisateur.
- En **AWAY**, l'éclairage et le pilotage de la fenêtre sont désactivés. La détection d'une présence déclenche une alarme.

Le projet est organisé autour de scénarios fonctionnels permettant d'aborder progressivement les différents périphériques du STM32. Ces scénarios ne constituent pas des projets indépendants. Chaque nouveau scénario vient ensuite enrichir progressivement votre application.

---

### Architecture Fonctionnelle du système

**Capteurs et boutons → acquisition et événements → décisions → actionneurs et supervision.**

Le STM32 constitue l'unité centrale. Les acquisitions périodiques (LDR, MQ2, Steam, AHT20) sont cadencées par une base temporelle ; les événements externes (PIR, boutons) sont gérés par interruption. Les actionneurs (buzzer, LEDs, servo) matérialisent les décisions de l'application. L'UART et le LCD assurent la supervision.

Les alarmes sont signalées par un signal sonore commun et par des LED distinctes permettant d'identifier leur origine.

---

### Suivi et évaluation du projet

L'évaluation du projet ne porte pas uniquement sur le fonctionnement final de la maquette.

#### 1. Validations intermédiaires

Au cours des séances, pour chaque scénario, vous devrez :

- présenter et valider la fonctionnalité développée ;
- expliquer et justifier vos choix techniques : brochage, configuration du périphérique, mode de fonctionnement et programmation ;
- montrer votre compréhension des algorithmes et périphériques utilisés ;
- Votre autonomie, votre capacité à analyser et résoudre les problèmes, ainsi que votre implication seront également prises en compte.

#### 2. Démonstration finale

Préparez une démonstration courte et reproductible permettant de valider les différents scénarios et leur intégration dans le système. Chaque membre du groupe pourra être interrogé sur les choix réalisés. Vous devrez être capables de faire le lien entre le besoin fonctionnel, la configuration matérielle et le code développé.

#### 3. Livrables finaux

À l'issue du projet, vous devrez déposer :

1. un rapport au format PDF ;
2. une archive ZIP du projet STM32CubeIDE complet, compilable et fonctionnel.

Le rapport présentera de manière synthétique : l'architecture du système et le brochage retenu ; les périphériques STM32 configurés ; les scénarios réalisés ; les résultats et les principales difficultés rencontrées.

---

### Utilisation de l'IA

- Les outils d'IA et les ressources externes peuvent être utilisés comme aide à la compréhension, à la recherche documentaire ou au débogage.
- Tout code intégré au projet doit cependant être compris, justifié et expliqué.
- Un code fonctionnel que le groupe ne sait pas expliquer sera considéré comme non maîtrisé.

---

## 2 — Avant de commencer

### 1. Découvrir votre maquette Smart Home

Le projet s'appuie sur le KEYESTUDIO IoT Smart Home Kit, initialement conçu autour d'une carte ESP32.

La carte ESP32 fournie avec le kit ne sera pas utilisée pour le projet. Elle sera remplacée par **NUCLEO_F411RE**.

La maquette met à disposition différents composants qui seront progressivement exploités au cours du projet.

### 2. Construire et adapter la maquette

Suivre le tutoriel « Guide de montage » afin d'assembler la structure mécanique de la maison et d'identifier les différents modules retenus.

Une breadboard supplémentaire sera nécessaire pour adapter et organiser les connexions entre la maison et la carte STM32.

> Référence : `Guide_montage_Smart_Home_VF.pdf`

### 3. Avant de câbler toute la maquette

**Conseil — Ne câblez pas immédiatement l'ensemble des composants, mais progressivement.**

Avant d'affecter les broches de vos capteurs et actionneurs, consultez la documentation de votre carte et vérifiez les broches déjà utilisées par ses périphériques embarqués.

Vous devrez donc ensuite :

- proposer le brochage permettant de connecter les différents capteurs, boutons et actionneurs au STM32 ;
- vérifier la compatibilité électrique des composants avec votre carte ;
- identifier les périphériques STM32 nécessaires : GPIO, EXTI, ADC, Timer, PWM, UART, I²C... ;
- organiser proprement le câblage de la maquette.

> Référence : `um1724-stm32-nucleo64-boards-mb1136-stmicroelectronics.pdf`

### 4. Avant de programmer : comprendre votre composant

Le wiki Keyestudio constitue une première ressource pour comprendre le fonctionnement des modules. Elle contient également des exemples pour Arduino.

L'objectif n'est pas de reproduire le code, mais de comprendre ce que fournit le composant, puis de déterminer comment l'interfacer et le piloter avec STM32.

Pour les composants nécessitant un protocole ou une séquence de commandes la datasheet devient la documentation de référence.

Avant toute programmation, vous devez être capable de :

- identifier la fonction physique du composant : que mesure-t-il ou que commande-t-il ?
- identifier son alimentation, ses broches et les niveaux électriques à respecter ;
- déterminer la nature de son interface : signal logique, analogique, PWM, I²C, etc. ;
- rechercher les caractéristiques temporelles utiles : période, durée d'impulsion, temps de réponse, temps de conversion... ;
- pour un composant numérique, comprendre la séquence de communication, les commandes et le format des données ;
- identifier le périphérique STM32 adapté et justifier la configuration retenue.

Vous devez ensuite être capable d'établir la chaîne :

```
Composant → signal/interface → périphérique STM32 → configuration CubeMX → fonctions HAL → traitement dans l'application
```

---

## Scénario 1 — Base temporelle pour l'échantillonnage

### 1. Gestion d'une tâche périodique — Timer et interruptions

Le système doit réaliser périodiquement l'acquisition des capteurs et la mise à jour de son état.

- Configurez un Timer du STM32 afin de générer une interruption périodique, avec une période comprise entre 2 et 5 secondes.
- Justifiez les paramètres de configuration choisis.
- Validez expérimentalement le fonctionnement du Timer avec la LED PA5 et d'un oscilloscope.

Cette base de temps permettra ensuite de cadencer les acquisitions des différents capteurs : luminosité, gaz, pluie, température et humidité.

Le fonctionnement général attendu est :

```
Timer → événement d'acquisition → capteurs → traitement / décision → actionneurs → LCD / UART
```

---

## Scénario 2 — Sécurité : HOME/AWAY et détection d'intrusion

### Gestion des modes HOME / AWAY — GPIO et EXTI

Le système possède deux modes :
- **HOME** : la maison est occupée
- **AWAY** : la maison est inoccupée

Au démarrage, le système est en mode HOME par défaut. Un bouton poussoir MODE permet de basculer entre les deux modes.

Utiliser le **Bouton BT User** de la carte. Il sera géré à l'aide d'une interruption externe (EXTI).

Attention au comportement réel du bouton et à la nécessité éventuelle de mettre en œuvre un anti-rebond.

Le changement de mode devra être pris en compte par l'ensemble du système et signalé via UART et/ou sur le LCD.

### Détection d'intrusion — PIR et alarme

Le capteur PIR (Passive Infra-Red sensor) permet de détecter un mouvement dans la maison. Il s'agit d'un capteur numérique à deux états : bas au repos et haut lorsqu'un mouvement est détecté. Le signal reste haut pendant un délai aléatoire avant de revenir à l'état bas.

Configurez-le en interruption externe (EXTI) pour détecter cet événement.

Le comportement du système dépend du mode courant :

- **HOME** : une détection PIR est considérée comme normale et ne déclenche pas d'alarme.
- **AWAY** : une détection PIR déclenche une alarme intrusion.

En cas d'intrusion, le système doit :

- activer une LED d'alarme « Intrusion » et produire une alarme sonore à l'aide du buzzer ;
- faire clignoter la LED tant que l'alarme intrusion est active ;

La signalisation de l'alarme devra être réalisée de manière **non bloquante**.

En cas d'intrusion, le système doit signaler l'événement via UART et/ou sur le LCD.

> Ressources :
> - [PIR Motion Sensor](https://wiki.keyestudio.com/Ks0052_keyestudio_PIR_Motion_Sensor)
> - [Buzzer passif](https://wiki.keyestudio.com/Ks0019_keyestudio_Passive_Buzzer_module)
> - [LED Module](https://wiki.keyestudio.com/KS0016_Keyestudio_White_LED_Module)

---

## Scénario 3 — Détection de gaz

Le capteur MQ-2 permet de détecter différents gaz et fumées. Il permet de surveiller la qualité de l'air et de détecter une situation anormale.

Sa sortie analogique permet d'évaluer le niveau de détection et sera acquise périodiquement à l'aide de l'ADC.

- Configurer l'ADC afin de mesurer la sortie analogique du MQ-2.
- Caractériser expérimentalement le capteur et définissez un seuil de déclenchement de l'alarme adapté au montage.
- Lorsque la valeur mesurée dépasse le seuil défini :
  - a. activer une LED d'alarme « Gas » et produire une alarme sonore à l'aide du buzzer ;
  - b. faire clignoter la LED tant que l'alarme est active ;

La signalisation de l'alarme devra être réalisée de manière **non bloquante**.

La surveillance du gaz constitue une fonction de sécurité et doit rester **active en permanence**.

En cas d'alarme, un message explicite de type « GAS ALERT » devra être transmis sur l'UART et affiché sur le LCD.

> Ressources :
> - [Gas Sensor MQ-2](https://wiki.keyestudio.com/Ks0042_keyestudio_MQ-4_Gas_Sensor)
> - [Buzzer passif](https://wiki.keyestudio.com/Ks0019_keyestudio_Passive_Buzzer_module)
> - [LED Module](https://wiki.keyestudio.com/KS0016_Keyestudio_White_LED_Module)

---

## Scénario 4 — Supervision

### Supervision du système — Liaison série UART

Le système doit pouvoir communiquer avec un PC afin de suivre son fonctionnement.

- Identifier la liaison UART reliée au ST-LINK.
- Configurez la liaison UART et afficher un message de bienvenue au démarrage.
- Validez la communication avec un terminal série sur le PC.

La liaison série pourra être utilisée librement pour le diagnostic et le débogage pendant le développement.

La liaison UART sera ensuite utilisée pour transmettre les principales informations de supervision du système (Mesures, Mode, Alarmes, etc.)

À terme, UART et LCD devront permettre de visualiser l'état global du système et ses principales mesures.

---

## Scénario 5 — Gestion intelligente de la fenêtre

La maison possède une fenêtre motorisée commandée par un servomoteur. Son fonctionnement prend en compte la commande de l'utilisateur, la détection de pluie et le mode global HOME/AWAY.

### 1. Commande du servomoteur — PWM

- Configurer un Timer en mode PWM afin de commander le servomoteur.
- Caractériser expérimentalement son fonctionnement et déterminez les paramètres correspondant aux deux positions : OPEN ↔ CLOSED
- Valider les deux positions avant d'intégrer le servomoteur au reste du système.

### 2. Commande manuelle — Bouton

Un bouton « Fenêtre » permet à l'utilisateur de demander l'ouverture ou la fermeture de la fenêtre.

Configurer et programmer le bouton afin de gérer cette fonctionnalité.

Vous êtes libres de choisir la méthode de détection de l'appui. Le choix réalisé devra être justifié.

### 3. Fermeture automatique — Détection de pluie

Le Steam Sensor est un capteur analogique qui peut être utilisé comme détecteur de pluie. Lorsque la quantité d'humidité présente à sa surface augmente, sa tension de sortie augmente.

- Acquérir périodiquement sa sortie analogique à l'aide de l'ADC.
- Caractériser expérimentalement le capteur et définir un seuil permettant de détecter une condition de pluie.
- Lorsqu'une pluie est détectée, la fenêtre doit être automatiquement fermée et rester fermée tant que cette condition persiste.
- De même, en mode AWAY, la fenêtre doit rester fermée.

L'état de la fenêtre devra être signalé via UART et/ou sur le LCD : `WINDOW OPEN` / `WINDOW CLOSED`

> Ressources :
> - [Steam Sensor](https://wiki.keyestudio.com/Ks0203_keyestudio_Steam_Sensor)
> - [Digital Push Button](https://wiki.keyestudio.com/Ks0029_keyestudio_Digital_Push_Button)
> - [Micro Servo](https://wiki.keyestudio.com/Ks0194_keyestudio_Micro_Servo)
> - [LED Module](https://wiki.keyestudio.com/KS0016_Keyestudio_White_LED_Module)

---

## Scénario 6 — Mesure de température

### TMP102 — Communication I²C et développement du driver

La température ambiante est mesurée à l'aide du capteur TMP102, qui communique avec le STM32 via le bus I²C.

L'objectif est de développer un driver élémentaire à partir de la documentation technique du composant, **sans utiliser de bibliothèque prête à l'emploi**.

### 1. Configuration de l'I²C

Configurer le périphérique I²C nécessaire à la communication avec le TMP102.

Identifiez notamment les broches SCL et SDA, l'adresse I²C du capteur et les principaux paramètres nécessaires à la communication.

### 2. Analyse de la documentation et développement du driver

À partir de la datasheet, identifier :

- l'adresse I²C du capteur ;
- la séquence nécessaire à son initialisation ;
- les commandes permettant de déclencher une mesure ;
- le format des données retournées ;
- la méthode permettant de convertir les données brutes en température (°C).

Implémentez au minimum les fonctions permettant :

- d'initialiser/configurer le capteur ;
- de déclencher une mesure ;
- de récupérer les données du capteur ;
- de convertir les données brutes en température.

Les valeurs de température devront être acquises périodiquement et affichées sur UART.

### 3. Observation et analyse des trames I²C

Pour les différentes étapes de communication avec TMP102, observer les signaux SCL et SDA à l'aide d'un analyseur logique.

Pour chaque échange, relever et analyser la trame obtenue.

> Référence : `tmp102.pdf`

### Optionnel : Gestion automatique de la fenêtre

Exploiter la température mesurée par le capteur pour commander automatiquement la fenêtre.

Lorsque les conditions climatiques le nécessitent, le système pourra demander l'ouverture de la fenêtre afin d'assurer une ventilation naturelle. Lorsque ces conditions ne sont plus réunies, il pourra demander sa fermeture.

Définir et justifier les seuils utilisés ainsi que la stratégie permettant de faire cohabiter cette commande automatique avec les commandes précédemment mises en place : bouton Fenêtre et détection de pluie.

Les valeurs de température devront être acquises périodiquement et affichées sur UART.

---

## Scénario 7 — Intégration du système & Affichage Local

À ce stade, les différentes fonctionnalités développées dans les scénarios précédents doivent être assemblées dans une application unique et cohérente. Elles ne doivent pas rester sous forme de programmes indépendants.

### Optionnel : Affichage local — LCD I²C

Le module Keyestudio LCD 1602 I²C est un afficheur comportant 2 lignes de 16 caractères et communiquant avec le STM32 via le bus I²C.

La bibliothèque permettant de piloter le LCD est fournie.

Analyser son interface, intégrer-la à votre projet puis utiliser-la pour afficher de manière claire les informations pertinentes du système.

Vous êtes libres de définir l'organisation et la stratégie d'affichage les plus adaptées aux contraintes d'un écran 16×2.

> **Note :** L'écran LCD et l'AHT20 partagent le même bus I²C. Vérifiez leurs adresses I²C respectives.

> Ressources :
> - [Module LCD](https://wiki.keyestudio.com/Ks0061_keyestudio_1602_I2C_Module)
> - Bibliothèque fournie : `LCD_I2C.zip`
