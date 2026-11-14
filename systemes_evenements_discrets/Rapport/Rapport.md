<!-- nohf -->
# Rapport — Régulateur / Limiteur de vitesse et kit mains-libres Bluetooth

**Auteurs** : *à compléter* · **Cours** : Systèmes à événements discrets — ESIEE Paris · **Date** : *à compléter*

> **Mode d'emploi de ce document.** Chaque titre est suivi d'une note en *italique* qui rappelle ce que la section doit contenir, puis, quand c'est utile, d'un encadré « Schéma à insérer » qui précise le diagramme attendu. Ces notes et ces encadrés sont à supprimer au fur et à mesure de la rédaction.

<!-- newpage -->

# Table des matières

<!-- TOC -->

<b>• [Rapport — Régulateur / Limiteur de vitesse et kit mains-libres Bluetooth](#rapport--r%C3%A9gulateur--limiteur-de-vitesse-et-kit-mains-libres-bluetooth)</b><br>
<b>• [Table des matières](#table-des-mati%C3%A8res)</b><br>
<b>• [1. Introduction et démarche](#1-introduction-et-d%C3%A9marche)</b><br>
<b>• [1.1 Contexte et objectif](#11-contexte-et-objectif)</b><br>
<b>• [1.2 Démarche méthodologique](#12-d%C3%A9marche-m%C3%A9thodologique)</b><br>
<b>• [1.3 Outils et environnement de travail](#13-outils-et-environnement-de-travail)</b><br>
<b>• [1.4 Conventions de modélisation](#14-conventions-de-mod%C3%A9lisation)</b><br>
<b>• [2. Architecture générale du système](#2-architecture-g%C3%A9n%C3%A9rale-du-syst%C3%A8me)</b><br>
<b>• [2.1 Vue d'ensemble et découpage en sous-systèmes](#21-vue-densemble-et-d%C3%A9coupage-en-sous-syst%C3%A8mes)</b><br>
<b>• [2.2 Responsabilités et interfaces des composants](#22-responsabilit%C3%A9s-et-interfaces-des-composants)</b><br>
<b>• [2.3 Stratégie de communication entre composants](#23-strat%C3%A9gie-de-communication-entre-composants)</b><br>
<b>• [2.4 Cycle de vie et modes globaux](#24-cycle-de-vie-et-modes-globaux)</b><br>
<b>• [2.5 Choix de conception transverses](#25-choix-de-conception-transverses)</b><br>
<b>• [3. Modélisation et simulation du véhicule](#3-mod%C3%A9lisation-et-simulation-du-v%C3%A9hicule)</b><br>
<b>• [3.1 Rôle du simulateur et modèle de base fourni](#31-r%C3%B4le-du-simulateur-et-mod%C3%A8le-de-base-fourni)</b><br>
<b>• [3.2 Identification des interfaces des modules véhicule](#32-identification-des-interfaces-des-modules-v%C3%A9hicule)</b><br>
<b>• [3.3 Module de dynamique du déplacement](#33-module-de-dynamique-du-d%C3%A9placement)</b><br>
<b>• [3.4 Gestion de la pédale d'accélération](#34-gestion-de-la-p%C3%A9dale-dacc%C3%A9l%C3%A9ration)</b><br>
<b>• [3.5 Intégration du système de freinage](#35-int%C3%A9gration-du-syst%C3%A8me-de-freinage)</b><br>
<b>• [3.6 Mise en route, arrêt et arrêt d'urgence](#36-mise-en-route-arr%C3%AAt-et-arr%C3%AAt-durgence)</b><br>
<b>• [3.7 Tableau de bord de simulation](#37-tableau-de-bord-de-simulation)</b><br>
<b>• [3.8 Extensions apportées au modèle de base](#38-extensions-apport%C3%A9es-au-mod%C3%A8le-de-base)</b><br>
<b>• [3.9 (Optionnel) Modélisation énergétique](#39-optionnel-mod%C3%A9lisation-%C3%A9nerg%C3%A9tique)</b><br>
<b>• [3.10 Validation du simulateur](#310-validation-du-simulateur)</b><br>
<b>• [4. Spécification et développement du régulateur de vitesse](#4-sp%C3%A9cification-et-d%C3%A9veloppement-du-r%C3%A9gulateur-de-vitesse)</b><br>
<b>• [4.1 Principe de régulation et analyse du besoin](#41-principe-de-r%C3%A9gulation-et-analyse-du-besoin)</b><br>
<b>• [4.2 Interfaces du régulateur](#42-interfaces-du-r%C3%A9gulateur)</b><br>
<b>• [4.3 Architecture interne du régulateur](#43-architecture-interne-du-r%C3%A9gulateur)</b><br>
<b>• [4.4 Statechart du régulateur](#44-statechart-du-r%C3%A9gulateur)</b><br>
<b>• [4.5 Loi de commande du couple moteur](#45-loi-de-commande-du-couple-moteur)</b><br>
<b>• [4.6 Console d'affichage du régulateur](#46-console-daffichage-du-r%C3%A9gulateur)</b><br>
<b>• [4.7 Tests et validation sur route dénivelée](#47-tests-et-validation-sur-route-d%C3%A9nivel%C3%A9e)</b><br>
<b>• [5. Spécification et développement du limiteur de vitesse](#5-sp%C3%A9cification-et-d%C3%A9veloppement-du-limiteur-de-vitesse)</b><br>
<b>• [5.1 Principe et cahier des charges](#51-principe-et-cahier-des-charges)</b><br>
<b>• [5.2 Extension du statechart du régulateur](#52-extension-du-statechart-du-r%C3%A9gulateur)</b><br>
<b>• [5.3 Configuration de la consigne](#53-configuration-de-la-consigne)</b><br>
<b>• [5.4 Déclenchement différé, témoin et reprise de la main](#54-d%C3%A9clenchement-diff%C3%A9r%C3%A9-t%C3%A9moin-et-reprise-de-la-main)</b><br>
<b>• [5.5 Tests et validation](#55-tests-et-validation)</b><br>
<b>• [6. Kit mains-libres Bluetooth](#6-kit-mains-libres-bluetooth)</b><br>
<b>• [6.1 Modélisation du téléphone et du système audio](#61-mod%C3%A9lisation-du-t%C3%A9l%C3%A9phone-et-du-syst%C3%A8me-audio)</b><br>
<b>• [6.2 Préalables et jumelage](#62-pr%C3%A9alables-et-jumelage)</b><br>
<b>• [6.3 Connexion automatique d'un téléphone jumelé](#63-connexion-automatique-dun-t%C3%A9l%C3%A9phone-jumel%C3%A9)</b><br>
<b>• [6.4 Gestion d'une communication](#64-gestion-dune-communication)</b><br>
<b>• [6.5 Gestion audio](#65-gestion-audio)</b><br>
<b>• [6.6 Statechart du contrôleur Bluetooth](#66-statechart-du-contr%C3%B4leur-bluetooth)</b><br>
<b>• [6.7 Affichages au tableau de bord](#67-affichages-au-tableau-de-bord)</b><br>
<b>• [6.8 Tests et validation](#68-tests-et-validation)</b><br>
<b>• [7. Extension : régulateur de vitesse adaptatif](#7-extension--r%C3%A9gulateur-de-vitesse-adaptatif)</b><br>
<b>• [7.1 Modélisation de l'environnement](#71-mod%C3%A9lisation-de-lenvironnement)</b><br>
<b>• [7.2 Extension du régulateur classique](#72-extension-du-r%C3%A9gulateur-classique)</b><br>
<b>• [7.3 Statechart étendu](#73-statechart-%C3%A9tendu)</b><br>
<b>• [7.4 Tests et validation](#74-tests-et-validation)</b><br>
<b>• [8. Intégration et validation globale](#8-int%C3%A9gration-et-validation-globale)</b><br>
<b>• [8.1 Scénarios de bout en bout](#81-sc%C3%A9narios-de-bout-en-bout)</b><br>
<b>• [8.2 Cohabitation des fonctions et priorités](#82-cohabitation-des-fonctions-et-priorit%C3%A9s)</b><br>
<b>• [8.3 Bilan au regard des critères d'évaluation](#83-bilan-au-regard-des-crit%C3%A8res-d%C3%A9valuation)</b><br>
<b>• [8.4 Difficultés rencontrées et solutions](#84-difficult%C3%A9s-rencontr%C3%A9es-et-solutions)</b><br>
<b>• [9. Conclusion et perspectives](#9-conclusion-et-perspectives)</b><br>
<b>• [Annexes](#annexes)</b><br>
<b>• [A. Tableau des événements et des opérations](#a-tableau-des-%C3%A9v%C3%A9nements-et-des-op%C3%A9rations)</b><br>
<b>• [B. Captures des statecharts et des panneaux](#b-captures-des-statecharts-et-des-panneaux)</b><br>
<b>• [C. Répartition du travail](#c-r%C3%A9partition-du-travail)</b>

<!-- /TOC -->

<!-- newpage -->

# 1. Introduction et démarche

## 1.1 Contexte et objectif

*À contenir : le contexte automobile (confort de conduite, réduction de consommation, sécurité), les deux systèmes à spécifier — le régulateur/limiteur de vitesse et le kit mains-libres Bluetooth — et la nature du livrable attendu : des statecharts exécutables et animés, pas du code écrit à la main.*

## 1.2 Démarche méthodologique

*À contenir : la progression en quatre étapes imposée par l'énoncé (modélisation du véhicule, régulateur, limiteur, kit Bluetooth), le principe de modélisation incrémentale — une fonctionnalité développée, puis immédiatement testée et validée dans un panel — et la place donnée à l'extension « régulateur adaptatif ».*

## 1.3 Outils et environnement de travail

*À contenir : Rhapsody 9.0.2 et le projet support, les panneaux d'animation utilisés pour piloter et observer le système, l'API audio `PlaySound` pour les sons, et les constantes de temps de la simulation (période d'appel de 50 ms, échantillonnage de la régulation à 2 Hz).*

## 1.4 Conventions de modélisation

*À contenir : les règles de nommage des états, événements, opérations et attributs, les unités retenues (vitesse en km/h, pédales et couple moteur normalisés dans $[0\,;\,10]$, distances en mètres) et le vocabulaire commun utilisé dans tout le rapport.*

# 2. Architecture générale du système

*Cette partie est le cœur du rapport : elle explique d'abord la structure du système et la façon dont les objets collaborent, avant d'entrer dans le détail du comportement de chacun. Toutes les parties suivantes s'y réfèrent.*

## 2.1 Vue d'ensemble et découpage en sous-systèmes

*À contenir : la vue synthétique du projet sous forme de blocs — le véhicule simulé (dynamique et pédales), les fonctions d'aide à la conduite (régulateur, limiteur, régulateur adaptatif), le kit Bluetooth et l'interface homme-machine — avec une phrase par bloc décrivant sa responsabilité et son périmètre.*

> 📐 **Schéma à insérer** — Diagramme d'objets / de blocs Rhapsody : les sous-systèmes, leurs ports et leurs liens de communication.

## 2.2 Responsabilités et interfaces des composants

*À contenir : pour chaque composant, ce qu'il possède (état interne, décisions) et ce qu'il ne possède pas, puis ses entrées et ses sorties. L'objectif est de montrer que la dynamique du véhicule, la mesure de vitesse et l'action sur le couple restent dans le simulateur, et que les fonctions d'aide à la conduite ne produisent que des demandes (consignes, couple, freinage).*

> 📐 **Schéma à insérer** — Tableau ou diagramme des interfaces (entrées / sorties) par composant.

## 2.3 Stratégie de communication entre composants

*À contenir : le choix des mécanismes de communication (événements envoyés d'un objet à l'autre, opérations synchrones, attributs partagés), la manière dont le conducteur est modélisé (boutons, pédales, commandes au volant) et la façon dont les mesures remontent vers les fonctions de régulation. Un diagramme de séquence sur un cycle de régulation typique illustre bien cette partie.*

> 📐 **Schéma à insérer** — Diagramme de séquence : un cycle complet « mesure de vitesse → calcul → demande de couple → dynamique du véhicule ».

## 2.4 Cycle de vie et modes globaux

*À contenir : les états de haut niveau communs à tout le véhicule (contact coupé, contact mis moteur arrêté, moteur en marche, arrêt d'urgence), car ils conditionnent le fonctionnement de toutes les fonctions : pas de régulation sans moteur en marche, pas de jumelage Bluetooth moteur tournant.*

## 2.5 Choix de conception transverses

*À contenir : les décisions d'architecture qui ne sont pas explicitement demandées par l'énoncé mais qui structurent le projet — par exemple la séparation entre algorithmes de commande et machines d'états, la hiérarchisation des états pour éviter la duplication de transitions, la manière dont sont gérés les temporisateurs et les seuils (30 km/h, 500 ms, 200 ms). Chaque choix est justifié par un besoin ou par une conséquence observable en simulation.*

# 3. Modélisation et simulation du véhicule

## 3.1 Rôle du simulateur et modèle de base fourni

*À contenir : la description du modèle `BaseProjectVehicule2022` et de son découpage en deux modules comportementaux — `dyn_deplacement` pour le calcul périodique de la vitesse et de la distance, `gestion_pedales_moteur` pour l'acquisition des pédales et leur transformation en couple moteur — ainsi que la raison de ce découpage : permettre de tester le véhicule indépendamment des fonctions d'aide à la conduite.*

## 3.2 Identification des interfaces des modules véhicule

*À contenir : le tableau des entrées et des sorties des deux modules (throttle, positions de pédales, vitesse, distance, état moteur), la nature de chaque signal (périodique ou événementiel) et la période à laquelle il est rafraîchi. C'est la spécification d'interface demandée au point 1.1 de l'énoncé.*

> 📐 **Schéma à insérer** — Diagramme d'interfaces des modules `dyn_deplacement` et `gestion_pedales_moteur`.

## 3.3 Module de dynamique du déplacement

*À contenir : le comportement attendu du module (intégration de l'inertie, des frottements, de la résistance aérodynamique et du dénivelé de la route représenté par l'angle $\alpha$), la façon dont le statechart pilote le calcul périodique et le mécanisme de test retenu — un curseur agissant directement sur le throttle — pour vérifier la vitesse et la distance sans passer par les pédales.*

> 📐 **Schéma à insérer** — Statechart du module `dyn_deplacement` et capture du panneau de test.

## 3.4 Gestion de la pédale d'accélération

*À contenir : la conversion de la position de pédale `accelPedal` $\in [0\,;\,10]$ en couple moteur, la première version du module de commande et le panneau associé (curseur de pédale). On explique ici la loi de conversion choisie et son effet sur la dynamique du véhicule.*

## 3.5 Intégration du système de freinage

*À contenir : l'ajout de la pédale de frein, le temps de réponse de 200 ms, et surtout la priorité système « freinage > accélération » avec annulation de l'accélération en cas d'appui simultané. On décrit comment cette priorité est garantie par la structure du statechart et non par un simple test dans une action.*

## 3.6 Mise en route, arrêt et arrêt d'urgence

*À contenir : le cycle de vie du véhicule commandé par une clé de contact, les indicateurs moteur ON/OFF du tableau de bord, et le scénario d'arrêt d'urgence : coupure du contact pendant le déplacement, extinction immédiate du moteur, désactivation de la pédale d'accélération, maintien de l'inertie jusqu'à l'arrêt naturel par frottements ou par action sur le frein.*

## 3.7 Tableau de bord de simulation

*À contenir : la description du panel diagram livrable — curseur de throttle, curseur de pédale, vitesse instantanée, distance parcourue, état du moteur — et la logique de son organisation (lisibilité, séparation entre commandes et indicateurs).*

## 3.8 Extensions apportées au modèle de base

*À contenir : ce qui a été ajouté au modèle fourni et qui n'était pas explicitement demandé : fonctions de capteur, signaux supplémentaires exposés aux fonctions d'aide à la conduite, temporisations, alarmes. Pour chaque extension, on indique le besoin qui l'a motivée.*

## 3.9 (Optionnel) Modélisation énergétique

*À contenir : la consommation de carburant, la gestion et la charge de la batterie, ainsi que les indicateurs associés (jauge de carburant, niveau de batterie, métriques de consommation), si cette partie a été traitée.*

## 3.10 Validation du simulateur

*À contenir : les scénarios de test élémentaires (accélération seule, freinage seul, appui simultané, coupure du contact en mouvement) et les résultats obtenus — courbes de vitesse, comportement sur route dénivelée — qui serviront de référence pour valider les parties suivantes.*

<!-- newpage -->



# 4. Spécification et développement du régulateur de vitesse

## 4.1 Principe de régulation et analyse du besoin

*À contenir : le principe de la boucle fermée — comparaison permanente de la mesure de vitesse à la consigne, puis correction du couple moteur — et la synthèse du cahier des charges sous forme d'exigences numérotées : activation par bouton on/off, consigne initialisée à la vitesse courante, réglage par set+/set-, seuil de sécurité à 30 km/h, arrêt implicite ou explicite, suspension lors d'un appui sur l'accélérateur, console d'affichage.*

## 4.2 Interfaces du régulateur

*À contenir : les entrées du régulateur (mesure de vitesse, état des pédales, boutons on/off, set+ et set-, état du moteur) et ses sorties (demande de couple, état affiché, vitesse de consigne). On justifie le choix de faire produire au régulateur une demande de couple plutôt qu'une action directe sur le véhicule.*

> 📐 **Schéma à insérer** — Diagramme d'interfaces / de contexte du régulateur.

## 4.3 Architecture interne du régulateur

*À contenir : la décomposition retenue entre la machine d'états (qui décide « régule / ne régule pas / suspendu ») et le calcul de commande (qui produit le couple). On explique pourquoi ces deux aspects sont séparés, et comment la consigne et l'état courant sont mémorisés.*

## 4.4 Statechart du régulateur

*À contenir : la description des états et de leur hiérarchie (inactif, actif avec ses sous-états « régulation » et « suspendu par l'accélérateur »), des transitions et de leurs gardes (seuil de 30 km/h, appui frein, appui accélérateur, on/off), et la justification de la hiérarchisation choisie. Le statechart complet est inséré ici, accompagné d'un court paragraphe de lecture.*

> 📐 **Schéma à insérer** — Statechart du régulateur de vitesse (capture Rhapsody).

## 4.5 Loi de commande du couple moteur

*À contenir : l'algorithme de contrôle échantillonné à 2 Hz, la décomposition en un terme de régime permanent et un terme correctif proportionnel à l'écart de vitesse, ainsi que la saturation du couple dans les limites physiques du véhicule. On discute du comportement obtenu sur une route dénivelée ($-20° < \alpha < +20°$) et du rôle du terme de régime permanent pour compenser une pente.*

$$\text{erreur} = \frac{5 \times (v_{\text{consigne}} - v)}{6} \qquad \text{regime} = \frac{v_{\text{consigne}}}{12} \qquad \text{throttle} = \text{regime} + \text{erreur}$$

## 4.6 Console d'affichage du régulateur

*À contenir : les informations affichées (état du régulateur, vitesse de consigne) et la manière dont le panneau les restitue, en cohérence avec les indicateurs du tableau de bord décrits en 3.7.*

## 4.7 Tests et validation sur route dénivelée

*À contenir : les scénarios testés — montée, descente, changement de consigne, suspension puis reprise après accélération, désactivation par le frein et par le seuil de 30 km/h — et les résultats observés (courbes de vitesse, temps de réponse, dépassement éventuel).*

<!-- newpage -->

# 5. Spécification et développement du limiteur de vitesse

## 5.1 Principe et cahier des charges

*À contenir : la différence de nature entre réguler et limiter — le limiteur ne maintient pas une vitesse mais empêche de dépasser une consigne en agissant sur la pédale d'accélérateur — et les exigences associées : consigne réglable à tout moment, double appui modifiant la consigne de 10 unités en moins de 500 ms, pas de déclenchement si la vitesse courante est déjà supérieure à la consigne, mémorisation de la demande, témoin lumineux d'attente, alarme sonore en cas de désactivation inopinée.*

## 5.2 Extension du statechart du régulateur

*À contenir : la façon dont le mode limiteur est greffé sur le statechart existant plutôt que dupliqué — un état de choix de mode en amont, des sous-états spécifiques au limiteur, et le mécanisme garantissant l'exclusivité des deux fonctionnalités (à un instant donné, le conducteur a soit le régulateur, soit le limiteur). C'est la réponse à la question Q3.1.*

> 📐 **Schéma à insérer** — Statechart étendu régulateur + limiteur, avec le mode exclusif mis en évidence.

## 5.3 Configuration de la consigne

*À contenir : la gestion des boutons set+ et set-, la détection du double appui dans une fenêtre de 500 ms et la traduction en saut de 10 unités. On explique le choix de placer cette détection dans un état dédié (temporisation) plutôt que dans une action, et les bornes appliquées à la consigne.*

## 5.4 Déclenchement différé, témoin et reprise de la main

*À contenir : le comportement lorsque la vitesse est supérieure à la consigne — demande mémorisée, témoin allumé, déclenchement en différé dès que la vitesse repasse en dessous —, les deux façons de reprendre la main (bouton pause, appui vigoureux sur l'accélérateur) et le déclenchement de l'alarme sonore lors d'une désactivation inopinée, typiquement en forte descente.*

## 5.5 Tests et validation

*À contenir : les scénarios (activation sous la consigne, activation au-dessus, double appui, pause, reprise par l'accélérateur, descente provoquant une dérive) et les résultats observés, en vérifiant la cohérence avec les tests du régulateur.*

# 6. Kit mains-libres Bluetooth

## 6.1 Modélisation du téléphone et du système audio

*À contenir : le choix de modélisation selon la relation maître/esclave — le téléphone initie toujours la communication, le système audio du véhicule la subit —, la représentation de l'environnement (téléphone, contact, état du moteur) et la manière dont les événements externes (appel entrant, connexion établie) entrent dans le modèle.*

> 📐 **Schéma à insérer** — Diagramme des objets téléphone / kit et séquence de connexion.

## 6.2 Préalables et jumelage

*À contenir : les conditions préalables (contact mis, moteur arrêté, Bluetooth du téléphone actif) et le déroulement du jumelage : détection du « Kit Bluetooth Véhicule », validation par la commande « ok », saisie du code d'authentification (1234 par défaut), mémorisation du téléphone jumelé. On précise quelles conditions sont des gardes de transition et lesquelles relèvent du contexte global décrit en 2.4.*

## 6.3 Connexion automatique d'un téléphone jumelé

*À contenir : le scénario d'entrée dans le véhicule — mise du contact puis connexion automatique du téléphone déjà jumelé — et la redirection de toutes les communications vers le système audio, avec les conséquences sur l'état de la machine (témoin de connexion, disponibilité des commandes).*

## 6.4 Gestion d'une communication

*À contenir : les états d'une communication (appel entrant avec sonnerie, conversation, raccroché), les commandes au volant « ok » pour accepter et « annuler » pour refuser ou raccrocher, et la priorité de l'appel sur la source audio en cours d'écoute.*

## 6.5 Gestion audio

*À contenir : la stratégie audio (sonnerie synchronisée avec l'état « appel entrant », coupure puis restauration de la source en cours), son implémentation par `PlaySound` avec les fichiers `.wav` placés dans `CodesGeneres/ModeAnimation/`, et les précautions prises pour ne pas laisser une boucle sonore active après un changement d'état.*

## 6.6 Statechart du contrôleur Bluetooth

*À contenir : la vue d'ensemble de la machine — états « non jumelé », « jumelé », « connecté », avec les sous-états de la communication —, la justification de la hiérarchie retenue (le jumelage survit à la coupure du contact, la connexion non) et la lecture des transitions principales.*

> 📐 **Schéma à insérer** — Statechart complet du contrôleur Bluetooth.

## 6.7 Affichages au tableau de bord

*À contenir : les trois informations exigées (témoin de téléphone connecté, charge restante de la batterie, témoin de SMS non lus), la façon dont elles sont transportées depuis le contrôleur jusqu'à l'IHM et la manière dont elles sont présentées sur le panneau d'animation.*

## 6.8 Tests et validation

*À contenir : les scénarios de test (jumelage refusé faute de conditions, connexion automatique, appel accepté, appel refusé, raccrochage, perte de connexion), et les captures d'écran ou extraits de trace montrant les changements d'état attendus.*

<!-- newpage -->

# 7. Extension : régulateur de vitesse adaptatif

## 7.1 Modélisation de l'environnement

*À contenir : l'enrichissement du simulateur avec un véhicule précédent sur la même voie et un capteur avant (radar ou laser) fournissant la distance inter-véhicules, ainsi que la modélisation de la distance de sécurité souhaitée par le conducteur. On précise ce qui appartient à l'environnement (le véhicule précédent et sa vitesse) et ce qui appartient au véhicule piloté (capteur, consigne, commande).*

> 📐 **Schéma à insérer** — Diagramme de l'environnement : véhicule piloté, véhicule précédent, capteur de distance et flux associés.

## 7.2 Extension du régulateur classique

*À contenir : la logique de régulation adaptative — arbitrage entre la consigne de vitesse et la consigne de distance, ralentissement lorsque la distance de sécurité n'est plus respectée, retour à la vitesse de croisière mémorisée dès que la voie se libère — ainsi que la limitation volontaire de l'effort de freinage et l'alerte visuelle et sonore qui rend la main au conducteur en cas de freinage brusque.*

## 7.3 Statechart étendu

*À contenir : l'ajout des états et transitions propres à la régulation adaptative (régulation sur la vitesse, régulation sur la distance, alerte de freinage) et la manière dont ils s'insèrent dans la hiérarchie existante sans remettre en cause les comportements déjà validés.*

> 📐 **Schéma à insérer** — Statechart du régulateur adaptatif (extension du statechart de la section 4).

## 7.4 Tests et validation

*À contenir : les scénarios typiques — approche d'un véhicule plus lent, maintien de la distance, départ du véhicule précédent et reprise de la consigne, freinage brutal nécessitant l'intervention du conducteur — et les résultats observés.*

# 8. Intégration et validation globale

## 8.1 Scénarios de bout en bout

*À contenir : les scénarios qui font intervenir plusieurs fonctions à la fois (régulation puis appel téléphonique entrant, limitation en descente avec perte de vitesse, coupure du contact en cours de régulation) et le comportement observé, ce qui démontre la cohérence de l'architecture décrite en partie 2.*

## 8.2 Cohabitation des fonctions et priorités

*À contenir : la synthèse des règles de priorité entre fonctions — freinage sur accélération, appel sur source audio, exclusion mutuelle régulateur/limiteur, conducteur toujours prioritaire sur l'automatisme — et la manière dont ces règles sont réparties entre les statecharts et le contexte global.*

## 8.3 Bilan au regard des critères d'évaluation

*À contenir : une relecture du travail selon les quatre critères de l'énoncé — fonctionnalité et robustesse de l'architecture, clarté et hiérarchisation des statecharts, sémantique des noms (états, attributs, opérations, transitions), qualité du panel diagram — avec un jugement argumenté sur chaque point.*

## 8.4 Difficultés rencontrées et solutions

*À contenir : les problèmes effectivement rencontrés pendant le développement (gestion des temporisations, synchronisation entre panneaux et objets, effets de bord des transitions, saturation du couple) et la solution retenue pour chacun.*

# 9. Conclusion et perspectives

*À contenir : le bilan du projet — ce qui fonctionne, ce qui a été validé, les limites du modèle — et les prolongements possibles (régulation plus fine, gestion complète de l'énergie, autres fonctions d'aide à la conduite).*

<!-- newpage -->

# Annexes

## A. Tableau des événements et des opérations

*À contenir : le récapitulatif de tous les événements et opérations du modèle, avec leur émetteur, leur destinataire et leur rôle. Ce tableau sert de référence pour la lecture des statecharts.*

## B. Captures des statecharts et des panneaux

*À contenir : les captures Rhapsody en pleine résolution, regroupées par sous-système, pour celles qui n'ont pas été insérées dans le corps du rapport.*

## C. Répartition du travail

*À contenir : la répartition des tâches entre les membres du binôme et, le cas échéant, la chronologie du projet.*

