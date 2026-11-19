# Installare Alpha Monster

## macOS (10.15 o successivo, Apple Silicon e Intel)

1. Scarica `AlphaMonster-x.y.z-macOS.pkg`.
2. Il pacchetto non è firmato con un certificato Apple, quindi alla prima apertura macOS lo blocca. Per aprirlo:
   - fai **clic destro → Apri** sul file `.pkg` e conferma;
   - oppure vai in **Impostazioni di Sistema → Privacy e sicurezza** e premi **Apri comunque**.
3. Scegli cosa installare:
   - **App**: *Alpha Monster Mix* (con il modello IA) e *Alpha Monster Master* standalone, in `/Applications/Alpha Monster`;
   - **Audio Unit**: in `/Library/Audio/Plug-Ins/Components`;
   - **VST3**: in `/Library/Audio/Plug-Ins/VST3`.
4. Se un'app viene bloccata al primo avvio, usa di nuovo **clic destro → Apri**. Da Terminale puoi anche eseguire:
   ```
   xattr -dr com.apple.quarantine "/Applications/Alpha Monster"
   ```
5. Riavvia la DAW e, se serve, fai una nuova scansione dei plugin. In Logic Pro il plugin compare in *Audio Units → Alpha Monster*.

In alternativa usa lo `.zip`: contiene gli stessi bundle da copiare a mano nelle cartelle indicate sopra.

## Windows 10/11 (64 bit)

1. Scarica `AlphaMonster-x.y.z-Windows-Setup.exe` ed eseguilo.
2. Se compare SmartScreen ("Windows ha protetto il PC"), premi **Ulteriori informazioni → Esegui comunque**: l'installer non è firmato con un certificato commerciale.
3. Componenti installati:
   - *Alpha Monster Mix* e il modello IA in `C:\Program Files\Alpha Monster`;
   - il plugin VST3 in `C:\Program Files\Common Files\VST3\Alpha Monster Master.vst3`;
   - *Alpha Monster Master* standalone.
4. Riavvia la DAW e, se serve, fai una nuova scansione dei plugin VST3.

## Primo utilizzo

### Alpha Monster Mix

1. **CARICA REFERENCE** (oppure trascina il file nella finestra).
2. **ANALIZZA**: separa gli strumenti con l'IA. Il calcolo avviene tutto sul tuo computer (niente cloud): su un brano di 4 minuti servono indicativamente da 3 a 10 minuti, in base al processore. Su Apple Silicon è più veloce.
3. **TRACCE SEPARATE → AGGIUNGI TRACCE** con gli stem del tuo brano, oppure **MIX STEREO → CARICA MIX STEREO**.
4. **MIXA COME IL REFERENCE**, poi ascolta "Risultato Alpha Monster" e premi **ESPORTA**.
5. **SALVA ANALISI** salva l'analisi del reference (file `.amref`), così puoi riusarla senza rifare la separazione.

### Alpha Monster Master

Inseriscilo come ultimo plugin sul bus master. Poi:

1. **CARICA REF**;
2. **LEARN** mentre suona il tuo brano (meglio tutto il brano), poi premi di nuovo per fermare;
3. **MATCH** durante la riproduzione;
4. regola **Match EQ**, **Glue**, **Width** e **Ceiling** a orecchio.

Usa **ASCOLTA REF** per il confronto A/B.
