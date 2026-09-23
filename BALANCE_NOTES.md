# Ready Gamer — Notas de Balance

Referencia de todos los parámetros ajustables + heurísticas de diseño. Solo información — sin cambios de código.

## 1. Parámetros actuales (por archivo)

### Jugador — `src/sprites/SpritePlayer.c`
| Constante | Valor | Efecto |
|---|---|---|
| `PLAYER_MAX_HEALTH` | 1 | Muere de un golpe. Cualquier tuning de "vidas" pasa por aquí. |
| `INVINCIBILITY_FRAMES` | 60 (~1s) | **Código muerto en la práctica**: con 1 HP, `TakeDamage` mata en el primer golpe, nunca llega a un 2do golpe donde la invencibilidad importe. |
| `SHOOT_COOLDOWN` | 100 (~1.67s) | Cadencia de disparo. Alta comparada con el spawn de enemigos. |
| `WALK_ANIM_SPEED` | 8 | Solo visual. |

### Proyectiles — `SpriteScrew.c` / `ElectricProjectile.c`
| Constante | Valor |
|---|---|
| `SCREW_LIFETIME` / `PROJECTILE_LIFETIME` | 120 (~2s) |
| Daño normal / eléctrico | 1 / 2 (`PROJECTILE_DAMAGE_NORMAL/ELECTRIC`) |

### Enemigos — `src/sprites/*Virus.c`
`ENEMY_SPEED` = frames que debe esperar el temporizador antes de dar 1 paso → **valor bajo = más rápido**.

| Tipo | `ENEMY_SPEED` | HP base | Extra |
|---|---|---|---|
| BasicVirus | 10 | 3 | — |
| SpeedVirus | **5** | 3 | — |
| TankVirus | 20 | 5 | El más lento, fácil de kitear |
| BomberVirus | **5** | 3 | Dropea `Bomb` cada `BOMB_DROP_INTERVAL`=300 (~5s) |
| ChargeVirus | 12 (chase) | 3 | Windup 24f → dash 20 pasos de 2px, cooldown 100f |

⚠️ **Quirk detectado**: `SpeedVirus` y `BomberVirus` se mueven exactamente igual de rápido (ambos 5). "Speed" no se siente más rápido que el bombardero.

`Bomb.c`: fuse 240f (~4s), sin aviso visual previo más allá de su propio sprite (siempre vivo off-screen por `lim_x/y=255`).

### Economía — `CoinsPickup.c`, `Rooms.c`, `SpritePlayer.c`
| Fuente | Valor |
|---|---|
| Coin por kill | 1 (`SpriteScrew.c` / `ElectricProjectile.c`) |
| Coin pickup | 5, solo en room2 (x1) y room4 (x1) |
| Costo de puerta | 10 fijo, todas las salas |

Para abrir la 1ª puerta (room0) sin pickups, se necesitan **10 kills** solo de esa sala — pero `current_level` (y por ende `level_lengths`) ya viene subiendo, así que hay oleadas suficientes; el jugador nunca se queda corto de enemigos, pero sí de *tiempo* si el spawn timer es alto.

### Ritmo de oleadas — `StateGame.c`
| Constante | Valor |
|---|---|
| `ENEMY_SPAWN_DELAY` | 180 (~3s) entre spawns |
| `NEXT_ROUND_TIMER` | 300 (~5s) de espera tras limpiar oleada |
| `level_lengths[20]` | 2,3,3,3,4, 3,4,4,4,5, 5,5,5,6,6, 6,7,7,8,9 |
| Bonus de HP por nivel | `(current_level-1)/3`, tope +6 |

⚠️ **Quirk de compuesto doble**: `current_level` sube en **dos** eventos distintos — al limpiar oleada (`CheckForNextLevel`) *y* al entrar a la siguiente sala (`UPDATE()` en `pending_room_transition`). Si el jugador limpia rápido, la dificultad escala más rápido de lo que las 20 tablas de `level_spawns` fueron pensadas para reflejar por sala.

## 2. Principios de diseño: difícil-sin-frustrar

- **Telegraph antes de castigo**: todo golpe fuerte necesita aviso claro (ChargeVirus ya lo hace con blink; Bomb no tiene aviso visual fuerte, solo su sprite).
- **La muerte debe sentirse "mi culpa"**: si el jugador no pudo reaccionar a tiempo (frame-perfect, fuera de pantalla, RNG spawn encima suyo), es frustración, no dificultad.
- **Curva de dificultad = mezcla + cantidad, no solo velocidad/daño bruto**: subir HP a lo bruto se siente injusto si el jugador no tiene más herramientas; mejor introducir *patrones* nuevos gradualmente (ya lo hacen con `level_spawns`).
- **1 HP es una decisión de diseño extrema**: hace que absolutamente cualquier hit sea game over — con eso, "difícil pero justo" depende 100% de que los enemigos telegrafeen bien y las hitboxes sean generosas con el jugador, injustas con el enemigo. Vale la pena decidir explícitamente: ¿1 HP + iframes reales (2-3) o HP>1?
- **Economía como control de ritmo**: costo de puerta fijo + coin/kill fijo = control simple. Si querés runs más largas/exploratorias, subir costo o bajar income; si querés presión, lo inverso.
- **Evitar "spawn injusto"**: revisar que `GetRandomSpawnPositionFromTable` no coloque enemigos a distancia de ataque instantáneo del jugador al entrar a sala.
- **Fatiga por repetición**: mismo patrón de movimiento (persigue en línea recta con wall-avoidance) en 3 de 5 enemigos — la variedad percibida viene solo de velocidad/HP, no de comportamiento. ChargeVirus y BomberVirus son los únicos con mecánica distinta.
- **Loop infinito de niveles (wrap 20→1)**: sin curva de "run corta con final" ni recompensa creciente más allá de HP+6 tope — pensar si el loop necesita un gancho (más coins, cosmético, tiempo, leaderboard) para no sentirse plano tras la vuelta 1.
- **Testeo iterativo real**: los números per-se no bastan; jugar cada sala 5-10 veces buscando el punto exacto donde deja de sentirse "tenso" y pasa a "injusto" (normalmente: 2+ amenazas simultáneas fuera de tu control de cámara, o daño que no se puede telegrafear a tiempo dado tu movement speed).

## 3. Preguntas para vos al balancear

- ¿1 HP se queda o subimos a 2-3 con iframes reales?
- ¿Diferenciar más `SpeedVirus` de `BomberVirus` en velocidad?
- ¿Separar el incremento de `current_level` (oleada vs sala) o dejarlo compuesto a propósito?
- ¿Costo de puerta fijo (10) o escalado por sala/nivel?
- ¿Bomb necesita aviso visual (parpadeo previo a explotar) además del sprite?

## 5. Boss run — `StateBossRun` / `BossRunPlayer.c`

| Constante / sistema | Valor | Efecto |
|---|---|---|
| `BOSSRUN_STARTING_LIVES` | 2 | Vidas en boss run (separado del juego principal) |
| `INVINCIBILITY_FRAMES` | 60 (~1s) | I-frames tras golpe de bala |
| `RESPAWN_INVINCIBILITY_FRAMES` | 180 (~3s) | I-frames tras perder vida |
| `BULLET_SPAWN_INTERVAL` | 90 (~1.5s) | `StateBossRun.c` — cadencia de `BossBullet` |
| `AUTOSCROLL_FRAME_DIVIDER` | 2 | `CameraDriver.c` — ~30 px/s |
| Map size | 240×18 tiles | `include/BossRun.h` — corrido largo; contenido WIP |

**Reglas de paredes con i-frames (solo mapboss):** ver `SESSION_NOTES.md` / `CLAUDE.md` § Boss run. Interior = atravesable; filas 0 y 17 = sólidas; al terminar i-frames dentro de pared interior → empuje a la derecha.

**Tuning pendiente:** densidad de obstáculos en `mapboss`, patrones de balas, velocidad de scroll por tramo.

## 6. Build

ROM debug compilado y verificado: `bin/READY_GAMER_Debug.gb` (también en `Debug/rom.gb`). `ZGB.zip` se descomprimió en `ZGB_extracted/ZGB` (gitignored) para poder compilar — `ZGB_PATH` usado: `ZGB_extracted/ZGB/common`.
