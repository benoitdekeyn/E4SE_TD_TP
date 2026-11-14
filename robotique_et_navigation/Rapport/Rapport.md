# Rapport de Robotique Mobile et Navigation Autonome

> **Consigne de rédaction (à supprimer avant rendu).** Chaque titre est suivi d'une phrase en italique qui indique ce que la section doit contenir. Le rapport présente d'abord **l'approche** (choix, schémas, vue d'ensemble), puis descend ponctuellement dans les implémentations qui ne sont pas explicitement demandées par l'énoncé. Les emplacements `[Schéma : …]` / `[Capture : …]` marquent où insérer les figures.
>
> Les schémas peuvent être écrits en Mermaid (rendu automatique par MarkDownAble) : bloc ` ```mermaid ` puis `flowchart LR` ou `stateDiagram-v2`, par exemple.
>
> Sections à supprimer si le point n'a pas été traité : 9.4 (2D Nav Goal), 13.3 et 13.4 (améliorations A*), ou toute autre partie non réalisée.

## 1. Introduction

*Contexte du projet (robotique mobile, ROS, TurtleBot3), objectifs du TP et annonce du plan. Rester bref : une page maximum.*

## 2. Plateforme et environnement de travail

*Décrire les moyens utilisés avant d'entrer dans l'architecture.*

### 2.1 Le robot TurtleBot3 et sa chaîne matérielle

*Capteurs et actionneurs, rôle de l'OpenCR (odométrie, IMU, pilotage des Dynamixel) et de la Raspberry Pi (pont série → ROS via `rosserial`). Expliquer pourquoi la commande passe par `/cmd_vel` et la mesure par `/odom`.*

### 2.2 Simulation Gazebo et robot réel

*Ce qui change entre `turtlebot3_house.launch` et le TB3 du labo (horloge, réinitialisation, lidar LDS-01/02), et les précautions prises en conséquence.*

### 2.3 Outils et méthode de travail

*ROS/catkin, RViz, MobaXterm/SSH, scripts d'automatisation du dépôt (`tabdk.sh`, `.vscode/sync_robot.ps1`), workflow « coder → compiler → synchroniser → tester ». Justifier les choix d'outillage.*

## 3. Architecture générale du projet

*Le cœur du rapport : une vue d'ensemble avant tout détail. Toutes les figures de référence (graphe ROS, arborescence, TF) doivent être ici et être réutilisées dans les sections suivantes.*

### 3.1 Vue d'ensemble des nœuds et de leurs échanges

*[Schéma : graphe ROS complet (type `rqt_graph`) avec les 5 nœuds du package : `controleur`, `keyboard_input`, `client_services`, `client_action`, `detector`, et vis-à-vis des nœuds `turtlebot3_*`.] Pour chaque flux : nom du topic/service/action, type de message, sens et cadence. Un tableau récapitulatif est attendu.*

### 3.2 Organisation du package `TABDK`

*[Schéma : arborescence `src/`, `src/nodes/`, `launch/`, `config/`, `srv/`, `action/`.] Justifier le découpage du contrôleur en modules (`parametres`, `odometry`, `mouvements`, `states`, `modes`, `serveur_action`, `serveur_services`) et l'intérêt de séparer les nœuds « clients » (`keyboard_input`, `client_services`, `client_action`, `detector`) du contrôleur.*

### 3.3 Repères et arbre des transformations (TF)

*[Schéma : arbre `map → odom → base_footprint → base_link → base_scan`.] Expliquer quels nœuds publient quoi et pourquoi le contrôleur travaille dans le repère `odom`.*

### 3.4 Boucle de contrôle et cadencement

*[Schéma : organigramme de la boucle `while(ok())` du `main` : `modes.step()` → `serveur_action.step()` → `spinOnce()` → `publishStatus()` → `loop_rate.sleep()`.] Expliquer le choix d'une boucle unique à fréquence paramétrable, le rôle de `spinOnce`, et pourquoi toute la publication de `/cmd_vel` est centralisée dans `mouvements`.*

### 3.5 Diagramme de séquence d'un déplacement complet

*[Schéma : séquence depuis l'envoi de l'ordre (clavier, action ou consigne) jusqu'à l'arrêt du robot, en montrant la traversée machine à états → mouvements → `/cmd_vel` → OpenCR → `/odom` → contrôleur.] Ce schéma sert de fil conducteur pour les sections 7 à 11.*

## 4. Mise en place du package et du nœud contrôleur

*Création du package (dépendances), topic `/log` publié toutes les 2 s, compilation via `CMakeLists.txt` et validation avec `rosrun`, puis fichier `run.launch`. Expliquer le passage de `rosrun` à `roslaunch` (master automatique, chargement des paramètres) sans rédiger de tutoriel.*

## 5. Pilotage du TB3 en vitesse

*Premier niveau d'action sur le robot.*

### 5.1 Commande de vitesse

*Topic (`/cmd_vel`), namespace (`geometry_msgs`), type (`Twist`), signification des composantes `linear.x` et `angular.z`. Justifier la centralisation de la publication dans une classe `Mouvements` (`forward`, `backward`, `pivoter`, `move`, `stop`) plutôt que dans le `main` : un seul point de vérité sur `/cmd_vel`.*

### 5.2 Arrêt d'urgence et sécurité

*Stratégie de gestion du `SIGINT` (`init_options::NoSigintHandler` + `sigintHandler` qui lève `arret_demande`), arrêt propre du robot en fin de boucle et réinitialisation de la position. Expliquer pourquoi l'arrêt est traité dans la boucle et non dans le handler.*

## 6. Paramétrage de l'application

*Rendre le contrôleur générique : fréquence de contrôle, vitesses, gains, durées, mode par défaut. Comparer arguments de `launch` et fichier `params.yaml`, expliquer le choix du YAML chargé par `rosparam`, la classe `Params::load()` et les conversions `msToTicks`/`ticksToMs`. Mentionner l'intérêt d'exprimer les durées en multiples de la période.*

## 7. Gestion des modes : machine à états

*Le pilotage est structuré autour de modes et d'états, et non d'une suite de `if`.*

### 7.1 Architecture de la machine à états

*[Schéma : diagramme d'états (`stateDiagram-v2`) avec les états `STOP`, `FORWARD`, `BACKWARD`, `PIVOTER_*`, `REACH_POSITION*`, `REACH_ANGLE` et les transitions automatiques ou commandées.] Expliquer la séparation entre `Modes` (mode courant, pause, mode par défaut) et `States` (état interne et application de la commande), et le rôle des deux fonctions `updateFSM…()` / `applyState()`.*

### 7.2 Mode manuel

*Nœud `keyboard_input` (lecture clavier non canonique via `termios`, publication de `std_msgs/Int8` sur `/keyboard_input`), décodage dans le contrôleur, changement de vitesse à la volée et bornage aux valeurs maximales. Analyser la latence des E/S et justifier le découpage nœud clavier / nœud contrôleur (un nœud bloquant sur `getchar` ne doit pas bloquer la boucle de contrôle).*

### 7.3 Mode automatique et pause

*Mouvements périodiques `FORWARD`/`STOP`/`BACKWARD` cadencés en tics, mode par défaut choisi dans `params.yaml`, et état de pause global. Expliquer comment le mode courant est exposé aux autres nœuds (`/mode`, `/pause`).*

## 8. Localisation

*Comment le contrôleur obtient la position du robot.*

### 8.1 Odométrie et structures de données

*Topic `/odom`, fréquence, type `nav_msgs/Odometry`, structure `geometry_msgs/Pose2D` retenue. Rôle de `tf::getYaw` pour convertir le quaternion, calcul de la distance parcourue par accumulation, et intérêt des données de `twist`. [Schéma : flux OpenCR → rosserial → `/odom` → `Odometry` → contrôleur.]*

### 8.2 Réinitialisation de la position

*Topic `/reset` pour le robot réel, service `/gazebo/reset_simulation` pour la simulation, et remise à zéro cohérente de l'état interne (odométrie, cible, action en cours). Expliquer pourquoi les deux mécanismes coexistent et comment le contrôleur choisit (`ServiceClient` et repli sur la publication).*

## 9. Navigation autonome : asservissements

*Du cas le plus simple (une distance) au plus général (un point dans le plan). Présenter les principes de régulation avant les formules.*

### 9.1 Asservissement en distance (axe x)

*Principe du correcteur proportionnel sur l'erreur de position, saturation de la commande à `linear_speed`, tolérance d'arrêt `precision_linear`. [Schéma : bloc de régulation.] [Courbes : x(t) et v(t) pour x1 > 0 et x1 < 0.] Discuter l'écart résiduel à l'arrêt et la nécessité (ou non) d'un terme intégral.*

### 9.2 Asservissement en orientation

*Régulateur proportionnel sur l'erreur angulaire, normalisation du `delta_theta` dans [-π, +π] (fonction `cal_delta_Theta`), tolérance `precision_angular`, saturation à `angular_speed`. [Courbes : θ(t) pour plusieurs consignes, dont ±π.]*

### 9.3 Cinématique inverse : atteindre un point (x, y)

*Distance euclidienne et `atan2`, puis combinaison d'une rotation et d'une translation (ou commande simultanée). Expliquer le seuil `trigger_pivot` (pivot bloquant versus correction d'angle pendant l'avancement) et le fonctionnement de `reachPosition`, `reachPositionSmooth` et `reachPositionSmoothBack`. [Schéma : géométrie du problème (erreur de distance, erreur de cap).]*

### 9.4 Consigne de navigation interactive (2D Nav Goal)

*Topic de la consigne envoyée depuis RViz et intégration au contrôleur pour permettre une nouvelle cible en temps réel. Préciser comment une nouvelle consigne interrompt le déplacement en cours.*

## 10. Perception de l'environnement au Lidar

*Chaîne de détection, du message `sensor_msgs/LaserScan` jusqu'à l'affichage RViz.*

### 10.1 Données du Lidar

*Topic, type, caractéristiques du LDS-01/02 (360°, 3,5 m) et sémantique des champs `ranges`, `angle_min`, `angle_increment`… Traitement des mesures non valides (`< range_min`, `> range_max`, `inf`).*

### 10.2 Détection par 8 régions

*[Schéma : découpage angulaire en 8 secteurs autour du robot.] Principe : affectation des indices de `ranges` à chaque secteur, filtrage, recherche du minimum (fonction fournie dans `detector.cpp`), comparaison à un seuil et stockage du résultat par région.*

### 10.3 Visualisation dans RViz

*`visualization_msgs/MarkerArray` : marqueurs d'obstacles, labels textuels, code couleur rouge / vert-bleu. Expliquer le fichier `config/tb3_lidar.rviz` et le launch `rviz_tb3.launch`. [Capture : RViz avec détection active.]*

## 11. Services et actions

*Choisir le bon mécanisme de communication pour chaque besoin. Commencer par un tableau comparatif topic / service / action.*

### 11.1 Service de changement de mode (`/set_mode`)

*Format `.srv` (requête `mode`, réponse `success` et `mode_courant`), validation côté serveur et application via la machine à états. Justifier un service (configuration ponctuelle et synchrone) plutôt qu'un topic.*

### 11.2 Service de contrôle global (`/global_control`)

*Commandes `play`, `pause` et `reset` : rôle, cohérence avec la machine à états et orchestration du reset (annulation de l'action, remise à zéro, réinitialisation de l'odométrie et de la simulation).*

### 11.3 Action de déplacement (`Move.action`)

*[Schéma : séquence `client_action` → serveur `move` (`SimpleActionServer`) → machine à états → `/cmd_vel`, avec goal / feedback / result.] Détail du `.action` (cible x, y ; feedback position, vitesse, distance restante, temps, mode, pause ; résultat atteint, temps, distance, position) et justification de l'action : déplacement long, retour d'avancement continu, annulation.*

### 11.4 Annulation, préemption et cas limites

*Comment le serveur traite une nouvelle cible pendant un déplacement, une annulation ou un reset global ; pourquoi le compteur de tics s'arrête pendant la pause et pourquoi le serveur d'action est recréé après un reset (horloge Gazebo). [Schéma : états d'un goal côté actionlib.]*

## 12. Cartographie (SLAM)

*Le SLAM sert à produire la carte utilisée ensuite par la navigation.*

### 12.1 Construction de la carte avec Gmapping

*Méthode SLAM, conditions d'exploration (téléopération lente, balayage des recoins) et résultats en simulation et en réel. [Capture : carte en cours de construction dans RViz.]*

### 12.2 Sauvegarde et format de la carte

*`map_server` / `map_saver`, fichiers `.pgm` et `.yaml`, grille d'occupation (OGM) : zones libres, occupées et inconnues.*

## 13. Navigation autonome basée sur la carte

*Chaîne complète : carte → planification A* → suivi par le contrôleur.*

### 13.1 Planification de trajectoire (package `AStar-ROS`)

*[Schéma : chaîne `map_server` → `path_generator` (A*) → `/robotic_path` → contrôleur.] Principe de l'algorithme, entrées (carte, pose initiale, objectif), sortie `nav_msgs/Path` et paramètres de `AStar-ROS/config/params.yaml`.*

### 13.2 Suivi de trajectoire par le contrôleur

*Comment le chemin reçu est transformé en suite de waypoints : extraction des poses, avancement d'un waypoint à l'autre selon `waypoint_step`, passage par la machine à états (`REACH_POSITION_SMOOTH`, `REACH_POSITION_SMOOTH_BACK`) et gestion de l'arrêt final. [Schéma : boucle de suivi.]*

### 13.3 Amélioration : trajectoire plus lisse

*Principe de l'amélioration apportée (`path_simplifier.cpp` : simplification du chemin puis lissage) et son effet sur le suivi. [Schéma ou capture : chemin A* brut versus chemin simplifié.] Justifier les gains : moins de pivots, mouvement plus continu, meilleure précision.*

### 13.4 Amélioration : contournement d'obstacles dynamiques

*Stratégie retenue (détection lidar, seuils `debut_collision_face` / `debut_collision_dos`, temporisation `delay_collision`), déclenchement d'un évitement ou d'une replanification, puis reprise du chemin. [Schéma : logique d'évitement intégrée à la machine à états.]*

## 14. Résultats et validation

*Scénarios testés (simulation et robot réel), protocole, mesures (précision d'arrêt, temps de parcours, réactivité) et captures. Mentionner les difficultés rencontrées et comment elles ont été traitées.*

## 15. Bilan et perspectives

*Bilan des objectifs atteints, limites connues (erreur statique des correcteurs, portée de l'évitement local) et pistes d'amélioration : terme intégral, planificateur plus lisse, perception 3D, etc.*

## Annexes

*Arborescence complète du package, `params.yaml`, `run.launch`, liste des topics/services/actions, commandes de lancement (simulation et robot) et tout schéma complémentaire.*

