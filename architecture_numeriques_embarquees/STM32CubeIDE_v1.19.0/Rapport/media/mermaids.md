### 6.3. Machine à états HOME / AWAY

#### Description de la machine à états

````mermaid
stateDiagram
  direction TB
[*] --> HOME
  AWAY:AWAY
  HOME:HOME
  HOME --> AWAY:Bouton MODE appuyé
  AWAY --> HOME:Bouton MODE appuyé
````
### 6.5. Comportement selon le mode (AWAY vs HOME)

#### Machine à états avec alarme intrusion

```mermaid
stateDiagram
  direction TB
  [*] --> HOME
  HOME --> AWAY:Bouton MODE appuyé
  AWAY --> HOME:Bouton MODE appuyé
  HOME:<b>HOME :</b>
HOME:Alarme intrusion désactivée<br>
  AWAY:<b>AWAY</b>
AWAY:Surveillance alarme intrusion<br>
```

```mermaid
flowchart TD
    Surveillance([Système de surveillance en mode AWAY]) --> Condition_Mv{Présence détectée ?}
    Condition_Mv -- "true" --> Alarme[Déclenchement de\nl'alarme intrusion]
    Condition_Mv -- "false" --> Ignorer[On ne fait rien et on continue de vérifier]
    Ignorer --> Surveillance
```

### 10.12. Gestion automatique de la fenêtre par la température

#### Seuils et états de température

```mermaid
stateDiagram-v2
    [*] --> TEMPEREE
    TEMPEREE --> CHAUD : Temperature >= SEUIL_CHALEUR
    TEMPEREE --> FROID : Temperature <= SEUIL_FROID
    CHAUD --> TEMPEREE : Temperature < SEUIL_CHALEUR
    FROID --> TEMPEREE : Temperature > SEUIL_FROID
```
#### Résumé des comportements global de la fenêtre

```mermaid
stateDiagram
    state "HOME et Pluie non détectée" as HOME {
        state "fenêtre" as fenetre{
        state "OUVERTE" as OUVERTE
        state "FERMEE" as FERMEE
        OUVERTE --> FERMEE: Appui bouton fenêtre
        FERMEE --> OUVERTE: Appui bouton fenêtre  
        OUVERTE --> FERMEE: Température < Seuil de Chaleur
        FERMEE --> OUVERTE: Température > Seuil de Froid
        }
        
    }
```