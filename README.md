# Metinfo

## English

### Description
Metinfo is a utility program designed to extract ED2K hashes from .part.met files used by the eDonkey2000/Overnet and eMule peer-to-peer file sharing networks. The program automatically detects the file format version (14.0 or 14.1) and extracts the MD4 hash that uniquely identifies the file in the network, as well as all meta tags contained within the file.

### Installation
To compile and install metinfo, follow these instructions:

```bash
# Clone or download the source code
# Navigate to the project directory
cd metinfo

# Compile the program
make

# Optionally install system-wide (requires root privileges)
sudo make install
```

### Usage
```bash
# Basic usage (shows all meta tags)
./metinfo -f /path/to/file.part.met

# Display version information
./metinfo -V

# Display help information
./metinfo -h

# Show specific tag types
./metinfo -f /path/to/file.part.met -s  # Show only special tags
./metinfo -f /path/to/file.part.met -g  # Show only gap tags
./metinfo -f /path/to/file.part.met -t  # Show only standard tags
./metinfo -f /path/to/file.part.met -u  # Show only unknown tags

# Visualize download status
./metinfo -f /path/to/file.part.met -z

# Detailed output
./metinfo -f /path/to/file.part.met -v
```

### Script-Friendly Single Value Output
For script integration, metinfo provides options to output single values without labels or formatting:

```bash
# Get only the filename 
./metinfo -f /path/to/file.part.met -n
# Output: example_movie.mkv

# Get only the file size (in bytes)
./metinfo -f /path/to/file.part.met -S
# Output: 104857600

# Get only the ED2K hash
./metinfo -f /path/to/file.part.met -e
# Output: E7D81234AB56C890DEF12345ABC67890

# Get only the .part.met file version
./metinfo -f /path/to/file.part.met -m
# Output: 14.0

# Get only the number of meta tags
./metinfo -f /path/to/file.part.met -c
# Output: 42

# Get only the download progress percentage
./metinfo -f /path/to/file.part.met -p
# Output: 37.8
```

### JSON Output
All information can be output in JSON format for easy integration with other tools:

```bash
# Full JSON output
./metinfo -f /path/to/file.part.met -j

# Single value JSON output
./metinfo -f /path/to/file.part.met -e -j
# Output: {"ed2k_hash":"E7D81234AB56C890DEF12345ABC67890"}

./metinfo -f /path/to/file.part.met -m -j
# Output: {"format_version":"14.0"}
```

### Script Examples
```bash
# Check if a file is completely downloaded
if [ $(./metinfo -f /path/to/file.part.met -p) = "100.0" ]; then
    echo "Download complete!"
else
    echo "Download in progress..."
fi

# Collect information about a download
FILENAME=$(./metinfo -f /path/to/file.part.met -n)
SIZE=$(./metinfo -f /path/to/file.part.met -S)
PROGRESS=$(./metinfo -f /path/to/file.part.met -p)
HASH=$(./metinfo -f /path/to/file.part.met -e)

echo "File: $FILENAME"
echo "Size: $SIZE bytes"
echo "Progress: $PROGRESS%"
echo "Hash: $HASH"
```

### How It Works
The program reads the .part.met file and analyzes its structure:

1. It reads the first byte to determine the file format version:
   - 224 (0xE0) = Version 14.0 (eMule, eDonkey pre-0.49)
   - 225 (0xE1) = Version 14.1 (eDonkey/Overnet 0.49+)

2. It then positions the file pointer at the appropriate location for the ED2K hash:
   - Version 14.0: Position 5
   - Version 14.1: Position 6

3. It reads the 16 bytes of the MD4 hash and converts them to a hexadecimal string.

4. It reads and processes all meta tags in the file, including:
   - Special tags (with 1-byte names)
   - Gap tags (indicating undownloaded areas)
   - Standard tags (like artist, album, title)
   - Unknown tags

5. For visualization, it creates a map of downloaded and missing parts of the file.

### Command Line Options
```
Display options:
  -f, --file=FILE      Specify the .part.met file to analyze
  -a, --all            Show all tags (default)
  -s, --special        Show only special tags
  -g, --gap            Show only gap tags
  -t, --standard       Show only standard tags
  -u, --unknown        Show unknown tags

Specific fields (script-friendly, raw output):
  -n, --name           Show filename only
  -S, --size           Show file size only
  -d, --date           Show last seen complete date only
  -p, --progress       Show download progress only
  -e, --hash           Show ED2K hash only
  -m, --metversion     Show .part.met version only (14.0 or 14.1)
  -c, --tagcount       Show number of meta tags only

Output format:
  -j, --json           Output in JSON format

Other options:
  -v, --verbose        Show detailed information
  -V, --version        Show program version
  -z, --visualize      Visualize file download status
  -h, --help           Show this help message
```

### License
This program is provided as-is, without any express or implied warranty.

---

## metrepair (companion tool)

### Description
`metrepair` is a companion program, built alongside `metinfo`, that reproduces the core - purely local - part of what repair tools like MetMedic do for corrupted `.part.met` files: given the correct per-block MD4 hashes of a download (a "reference") and the actual downloaded data (a `.part` file), it re-hashes every block, tells you which ones are intact, and can write a brand new, valid `.part.met` with correct Gap/Filename/Filesize tags.

**It never contacts any eDonkey/eMule server or peer.** You still need to obtain the reference block hashes yourself first - typically by re-adding the same download once in eMule/aMule (which recreates a fresh, correctly hashed but 0%-downloaded `.part.met` from the network) - or from an `ed2k://` link that already embeds a `p=hash1:hash2:...` part-hash parameter.

### Usage
```bash
# Check which blocks of a .part file are actually intact, without writing anything
./metrepair verify --ref /path/to/good.part.met --data /path/to/corrupted.part

# Same, but using an ed2k link with a p= hash-set instead of a .part.met file
./metrepair verify --ref "ed2k://|file|movie.mkv|1234567|<hash>|p=<h1>:<h2>:.../|" --data /path/to/corrupted.part

# Rebuild a valid .part.met from the reference hashes and the corrupted data
./metrepair rebuild --ref /path/to/good.part.met --data /path/to/corrupted.part -o /path/to/fixed.part.met

# JSON output, for scripting
./metrepair verify --ref /path/to/good.part.met --data /path/to/corrupted.part --json
```

After `rebuild`, place your data file next to the new `.part.met`, named the same way but without `.met` (e.g. `1.part.met` needs a sibling `1.part`), then resume the download in eMule/aMule as usual.

### How It Works
1. Reads the reference's per-block MD4 hash array, ID hash, filename and filesize - either from a `.part.met` file (any of the versions `metinfo` understands: 14.0, 14.1, or the large-file 0xE2 variant) or from an `ed2k://` link's `p=` parameter.
2. Splits the `.part` data file into the same 9,728,000-byte blocks used by the `.part.met` format, computes the MD4 of each one (a from-scratch RFC 1320 implementation, since MD4 isn't in the standard C library), and compares it to the reference.
3. Merges contiguous mismatched/missing blocks into Gap ranges.
4. In `rebuild` mode, writes a new `.part.met` (14.0-compatible layout) with the correct ID hash, block hash array, Filename/Filesize/Transferred tags, and the freshly computed Gap tags.

Before doing any of this, `metrepair` runs its MD4 implementation against the standard RFC 1320 test vectors and refuses to proceed if they don't match - correctness there is the whole point of the tool.

### Command Line Options
```
  verify  --ref <file.part.met|ed2k-link> --data <file.part> [--json]
  rebuild --ref <file.part.met|ed2k-link> --data <file.part> -o <out.part.met> [--force] [--json]
  -h, --help       Show help
  -V, --version    Show version

  -r, --ref=REF        Reference: a .part.met file, or an ed2k link with p=...
  -d, --data=FILE      The .part file to verify/repair
  -o, --output=FILE    Where to write the rebuilt .part.met (rebuild only)
  -F, --force          Overwrite the output file if it already exists
  -j, --json            Output in JSON format
```

### License
This program is provided as-is, without any express or implied warranty.

---

## Latin

### Descriptio
metinfo est instrumentum ad extrahendum "ED2K tesserae" (nexus identificationis) ex "part.met" documentis quae in eDonkey2000/Overnet et eMule systemis communicationis inter pares (P2P) utuntur. Programma automatice formam documenti (14.0 vel 14.1) detegit et extrahit "MD4 tesseram" quae documentum in reti singulariter identificat, atque omnes meta indicationes in documento contentas.

### Installatio
Ad metinfo compilandum et installandum, has instructiones sequere:

```bash
# Codicem fontem cape vel descarga
# Ad directorium projecti naviga
cd metinfo

# Programma compila
make

# Si velis, installa in systemate (privilegia radicis requirit)
sudo make install
```

### Usus
```bash
# Usus fundamentalis (omnes meta indicationes monstrat)
./metinfo -f /via/ad/documentum.part.met

# Exhibe informationem de versione
./metinfo -V

# Exhibe auxilium
./metinfo -h
```

### Usus Pro Scriptis
Pro scriptis, metinfo praebet optiones ad producendum singulos valores sine etiquettis aut formatione:

```bash
# Solum nomen documenti obtine
./metinfo -f /via/ad/documentum.part.met -n
# Exitus: exemplum_pellicula.mkv

# Solum magnitudinem documenti obtine (in octettis)
./metinfo -f /via/ad/documentum.part.met -S
# Exitus: 104857600

# Solum ED2K tesseram obtine
./metinfo -f /via/ad/documentum.part.met -e
# Exitus: E7D81234AB56C890DEF12345ABC67890
```

### Exitus JSON
Omnis informatio potest in forma JSON produci:

```bash
# Plenus exitus JSON
./metinfo -f /via/ad/documentum.part.met -j

# Unius valoris exitus JSON
./metinfo -f /via/ad/documentum.part.met -e -j
# Exitus: {"ed2k_hash":"E7D81234AB56C890DEF12345ABC67890"}
```

### metrepair
`metrepair` est instrumentum additum quod partem localem instrumentorum reparationis (ut MetMedic) sine ulla conexione ad retem eDonkey/eMule praestat: datis tesseris MD4 correctis (ex documento `.part.met` bono, vel ex nexu `ed2k://` cum parametro `p=`) et documento `.part` corrupto, quodque frustum iterum tesserat, frusta integra a corruptis distinguit, et novum documentum `.part.met` validum scribere potest.

```bash
./metrepair verify  --ref /via/ad/bonum.part.met --data /via/ad/corruptum.part
./metrepair rebuild --ref /via/ad/bonum.part.met --data /via/ad/corruptum.part -o /via/ad/emendatum.part.met
```

### Licentia
Hoc programma "sicut est" praebetur, sine ulla garantia expressa vel implicita.

---

## Italiano

### Descrizione
metinfo è un programma di utilità progettato per estrarre gli hash ED2K dai file .part.met utilizzati dalle reti di condivisione file peer-to-peer eDonkey2000/Overnet ed eMule. Il programma rileva automaticamente la versione del formato del file (14.0 o 14.1) ed estrae l'hash MD4 che identifica in modo univoco il file nella rete, oltre a tutti i meta tag contenuti nel file.

### Installazione
Per compilare e installare metinfo, segui queste istruzioni:

```bash
# Clona o scarica il codice sorgente
# Naviga nella directory del progetto
cd metinfo

# Compila il programma
make

# Opzionalmente installa a livello di sistema (richiede privilegi di root)
sudo make install
```

### Utilizzo
```bash
# Utilizzo base (mostra tutti i meta tag)
./metinfo -f /percorso/al/file.part.met

# Visualizza informazioni sulla versione
./metinfo -V

# Visualizza informazioni di aiuto
./metinfo -h

# Visualizza tipi specifici di tag
./metinfo -f /percorso/al/file.part.met -s  # Mostra solo tag speciali
./metinfo -f /percorso/al/file.part.met -g  # Mostra solo tag gap
./metinfo -f /percorso/al/file.part.met -t  # Mostra solo tag standard
./metinfo -f /percorso/al/file.part.met -u  # Mostra solo tag sconosciuti

# Visualizza lo stato del download
./metinfo -f /percorso/al/file.part.met -z

# Output dettagliato
./metinfo -f /percorso/al/file.part.met -v
```

### Output per Script
Per l'integrazione con script, metinfo fornisce opzioni per produrre valori singoli senza etichette o formattazione:

```bash
# Ottieni solo il nome del file
./metinfo -f /percorso/al/file.part.met -n
# Output: esempio_film.mkv

# Ottieni solo la dimensione del file (in byte)
./metinfo -f /percorso/al/file.part.met -S
# Output: 104857600

# Ottieni solo l'hash ED2K
./metinfo -f /percorso/al/file.part.met -e
# Output: E7D81234AB56C890DEF12345ABC67890

# Ottieni solo la versione del file .part.met
./metinfo -f /percorso/al/file.part.met -m
# Output: 14.0

# Ottieni solo il numero di meta tag
./metinfo -f /percorso/al/file.part.met -c
# Output: 42

# Ottieni solo la percentuale di progresso del download
./metinfo -f /percorso/al/file.part.met -p
# Output: 37.8
```

### Output JSON
Tutte le informazioni possono essere prodotte in formato JSON per una facile integrazione con altri strumenti:

```bash
# Output JSON completo
./metinfo -f /percorso/al/file.part.met -j

# Output JSON di un singolo valore
./metinfo -f /percorso/al/file.part.met -e -j
# Output: {"ed2k_hash":"E7D81234AB56C890DEF12345ABC67890"}
```

### Esempi di Script
```bash
# Verifica se un file è completamente scaricato
if [ $(./metinfo -f /percorso/al/file.part.met -p) = "100.0" ]; then
    echo "Download completato!"
else
    echo "Download in corso..."
fi

# Raccogli informazioni su un download
NOME_FILE=$(./metinfo -f /percorso/al/file.part.met -n)
DIMENSIONE=$(./metinfo -f /percorso/al/file.part.met -S)
PROGRESSO=$(./metinfo -f /percorso/al/file.part.met -p)
HASH=$(./metinfo -f /percorso/al/file.part.met -e)

echo "File: $NOME_FILE"
echo "Dimensione: $DIMENSIONE byte"
echo "Progresso: $PROGRESSO%"
echo "Hash: $HASH"
```

### Opzioni della Linea di Comando
```
Opzioni di visualizzazione:
  -f, --file=FILE      Specifica il file .part.met da analizzare
  -a, --all            Mostra tutti i tag (default)
  -s, --special        Mostra solo i tag speciali
  -g, --gap            Mostra solo i tag gap
  -t, --standard       Mostra solo i tag standard
  -u, --unknown        Mostra i tag sconosciuti

Campi specifici (per script, output grezzo):
  -n, --name           Mostra solo il nome del file
  -S, --size           Mostra solo la dimensione del file
  -d, --date           Mostra solo la data dell'ultima volta visto completo
  -p, --progress       Mostra solo la percentuale di download
  -e, --hash           Mostra solo l'hash ED2K
  -m, --metversion     Mostra solo la versione del file .part.met (14.0 o 14.1)
  -c, --tagcount       Mostra solo il numero di meta tag

Formato di output:
  -j, --json           Output in formato JSON

Altre opzioni:
  -v, --verbose        Mostra informazioni dettagliate
  -V, --version        Mostra la versione del programma
  -z, --visualize      Visualizza lo stato del download
  -h, --help           Mostra questo messaggio di aiuto
```

### Licenza
Questo programma viene fornito così com'è, senza alcuna garanzia espressa o implicita.

---

## metrepair (strumento complementare)

### Descrizione
`metrepair` è un programma complementare, compilato insieme a `metinfo`, che replica la parte centrale - e puramente locale - di ciò che fanno strumenti di riparazione come MetMedic per i file `.part.met` corrotti: dati gli hash MD4 corretti per blocco di un download (un "riferimento") e i dati effettivamente scaricati (un file `.part`), ricalcola l'hash di ogni blocco, indica quali sono integri e può scrivere un nuovo `.part.met` valido con i tag Gap/Filename/Filesize corretti.

**Non contatta mai alcun server o peer eDonkey/eMule.** Gli hash di riferimento per blocco vanno comunque ottenuti prima in altro modo - tipicamente ri-aggiungendo una volta lo stesso download in eMule/aMule (che ricrea dalla rete un `.part.met` fresco, con hash corretti ma 0% scaricato) - oppure da un link `ed2k://` che include già un parametro `p=hash1:hash2:...`.

### Utilizzo
```bash
# Verifica quali blocchi di un file .part sono realmente integri, senza scrivere nulla
./metrepair verify --ref /percorso/al/buono.part.met --data /percorso/al/corrotto.part

# Come sopra, ma usando un link ed2k con hash-set p= invece di un file .part.met
./metrepair verify --ref "ed2k://|file|film.mkv|1234567|<hash>|p=<h1>:<h2>:.../|" --data /percorso/al/corrotto.part

# Ricostruisce un .part.met valido a partire dagli hash di riferimento e dai dati corrotti
./metrepair rebuild --ref /percorso/al/buono.part.met --data /percorso/al/corrotto.part -o /percorso/al/riparato.part.met

# Output JSON, per script
./metrepair verify --ref /percorso/al/buono.part.met --data /percorso/al/corrotto.part --json
```

Dopo `rebuild`, posiziona il file dati accanto al nuovo `.part.met`, con lo stesso nome ma senza `.met` (es. `1.part.met` richiede un `1.part` accanto), poi riprendi il download in eMule/aMule come di consueto.

### Come funziona
1. Legge l'array di hash MD4 per blocco, l'hash ID, il nome file e la dimensione dal riferimento - sia da un file `.part.met` (in qualsiasi versione compresa da `metinfo`: 14.0, 14.1 o la variante large-file 0xE2), sia dal parametro `p=` di un link `ed2k://`.
2. Divide il file dati `.part` negli stessi blocchi da 9.728.000 byte usati dal formato `.part.met`, calcola l'MD4 di ciascuno (implementazione da zero secondo RFC 1320, dato che MD4 non è nella libreria standard C) e lo confronta col riferimento.
3. Unisce i blocchi contigui mancanti/non corrispondenti in intervalli Gap.
4. In modalità `rebuild`, scrive un nuovo `.part.met` (formato compatibile 14.0) con l'hash ID corretto, l'array di hash per blocco, i tag Filename/Filesize/Transferred e i tag Gap appena calcolati.

Prima di fare tutto questo, `metrepair` testa la propria implementazione MD4 contro i vettori di test standard RFC 1320 e si rifiuta di procedere se non corrispondono - la correttezza qui è l'intero scopo dello strumento.

### Opzioni della Linea di Comando
```
  verify  --ref <file.part.met|link-ed2k> --data <file.part> [--json]
  rebuild --ref <file.part.met|link-ed2k> --data <file.part> -o <out.part.met> [--force] [--json]
  -h, --help       Mostra l'aiuto
  -V, --version    Mostra la versione

  -r, --ref=REF        Riferimento: un file .part.met, o un link ed2k con p=...
  -d, --data=FILE      Il file .part da verificare/riparare
  -o, --output=FILE    Dove scrivere il .part.met ricostruito (solo rebuild)
  -F, --force          Sovrascrive il file di output se già esistente
  -j, --json            Output in formato JSON
```

### Licenza
Questo programma viene fornito così com'è, senza alcuna garanzia espressa o implicita.
