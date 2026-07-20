# Undertow

Cliente BitTorrent nativo para Linux, escrito em C++17 com **Qt6** (interface) e
**libtorrent-rasterbar 2.x** (motor do protocolo BitTorrent).

## Recursos

- Adicionar torrents por arquivo `.torrent` ou link magnet, com pré-visualização
  dos arquivos antes de confirmar
- Lista de torrents com progresso, velocidade, seeds/peers, ETA, proporção
- Painel de detalhes por torrent: geral, rastreadores, peers, arquivos
  (com prioridade por arquivo: pular/baixa/normal/alta) e gráfico de velocidade
- Categorias personalizadas + filtros inteligentes na barra lateral
  (Tudo, Baixando, Semeando, Concluídos, Pausados, Com erro)
- Pausar/retomar/remover (com ou sem apagar os dados), forçar verificação,
  forçar reanúncio, copiar link magnet, abrir pasta de destino
- Limites de velocidade globais e por torrent; DHT, PEX (via libtorrent), LSD,
  UPnP e NAT-PMP configuráveis
- Persistência entre reinícios: cada torrent salva seu `.fastresume` (estado,
  prioridades, caminho) e é recarregado automaticamente ao abrir o programa
- Bandeja do sistema (minimizar ao fechar, notificações de conclusão/erro)
- Tema claro/escuro (paleta nativa via Qt Fusion, sem flicker de stylesheet)
- Integração com o sistema de arquivos: `.desktop` para abrir `.torrent` e
  links `magnet:` diretamente no Undertow

## Dependências (Ubuntu/Debian/Kali)

```bash
sudo apt install build-essential cmake pkg-config qt6-base-dev \
                 libtorrent-rasterbar-dev libssl-dev
```

Testado com Qt 6.4.2 e libtorrent-rasterbar 2.0.10 (repositórios padrão do
Ubuntu 24.04 / derivados, incluindo Kali).

## Compilar

```bash
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)
```

O binário fica em `build/undertow`.

## Instalar (opcional)

```bash
sudo cmake --install build
```

Isso copia o binário para `/usr/local/bin` e o `.desktop` para
`/usr/local/share/applications`. Para registrar o Undertow como aplicativo
padrão para `.torrent` e links `magnet:`:

```bash
xdg-mime default undertow.desktop application/x-bittorrent
xdg-mime default undertow.desktop x-scheme-handler/magnet
update-desktop-database ~/.local/share/applications 2>/dev/null || true
```

## Uso pela linha de comando

```bash
undertow                              # abre a interface
undertow /caminho/para/arquivo.torrent
undertow "magnet:?xt=urn:btih:..."
```

## Onde os dados ficam

- Dados de sessão (resume data por torrent, categorias, configurações):
  `~/.local/share/Undertow/`
- Preferências de janela/tema: `~/.config/Undertow.conf`

## Arquitetura

- `src/core/SessionManager.*` — único ponto de contato com libtorrent. Todo
  o resto do projeto (modelos, diálogos, janela principal) fala apenas em
  tipos Qt e nas structs simples de `TorrentRecord.h`, então trocar ou
  atualizar a versão do libtorrent no futuro fica isolado nesse arquivo.
- `src/models/` — `TorrentTableModel` (atualização incremental para preservar
  seleção a cada tick) e o delegate da barra de progresso.
- `src/ui/` — janela principal, diálogos de adicionar/configurações, painel
  de detalhes e o widget de gráfico de velocidade (desenhado à mão, sem
  dependências de gráficos externas).

O `SessionManager` expõe um timer de ~1s (`pollAlerts`) que drena os alerts
do libtorrent e reconstrói um snapshot (`QVector<TorrentRecord>`); a UI nunca
toca em tipos do libtorrent diretamente.

