# Fonte bitmap

`mono.png` e `mono.fnt` formam o atlas de texto do jogo: uma grade de glifos
rasterizados por [`tools/gen_assets.py`](../../tools/gen_assets.py) a partir da
**Liberation Mono Regular**, mais um arquivo de metadados com o tamanho da
célula, o número de colunas e a lista de caracteres na ordem do atlas.

Por serem derivados da Liberation Mono, esses dois arquivos **não** estão sob a
licença MIT do resto do projeto: eles seguem a **SIL Open Font License 1.1**,
cujo texto completo e avisos de copyright estão em [`OFL.txt`](OFL.txt).

"Liberation" é um Reserved Font Name sob a OFL, então os arquivos derivados aqui
não usam esse nome. Para regerar o atlas é preciso ter a fonte instalada
(`ttf-liberation` no Arch); o caminho está em `FONTE_SISTEMA`, no gerador.

Trocar por outra fonte é mudar esse caminho e rodar `python tools/gen_assets.py`
— e, se a nova fonte tiver outra licença, atualizar este diretório.
