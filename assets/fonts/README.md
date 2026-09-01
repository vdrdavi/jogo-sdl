# Fonte bitmap

`mono.png` e `mono.fnt` formam o atlas de texto do jogo: uma grade de glifos de
8×16 rasterizados por [`tools/gen_assets.py`](../../tools/gen_assets.py) a partir
de `unscii-16.ttf`, mais um arquivo de metadados com o tamanho da célula, o
número de colunas e a lista de caracteres na ordem do atlas.

## unscii

[unscii](http://viznut.fi/unscii/) é uma fonte bitmap Unicode criada por
**Viznut**, desenhada para grades de 8×16 e inspirada em fontes de sistemas
clássicos. As variantes `unscii-8` e `unscii-16` estão em **domínio público** —
só a variante `unscii-16-full` é GPL, por incorporar glifos do GNU Unifont, e
não é a que usamos aqui.

Como está em domínio público, o arquivo `unscii-16.ttf` acompanha o repositório:
regerar o atlas não depende de nenhuma fonte instalada no sistema.

Sendo uma fonte de pixels, o gerador reduz o alfa a 1 bit depois de rasterizar —
o antialiasing só borraria as bordas dos glifos.

## Trocar a fonte

Aponte `FONTE`, no começo do gerador, para outro arquivo e rode
`python tools/gen_assets.py`. O tamanho da célula sai da maior caixa envolvente
de todo o charset, então fontes de outras métricas funcionam sem ajuste manual.
Se a nova fonte não estiver em domínio público, atualize este diretório e a
seção de licença do [README](../../README.md) com os termos dela.
