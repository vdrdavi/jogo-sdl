#!/usr/bin/env python3
"""Gera os assets placeholder do projeto (texturas, fonte bitmap e sons).

Requer Pillow apenas para gerar as imagens; o jogo em si nao depende de nada
alem do SDL3.

O atlas de texto e rasterizado da unscii-16, uma fonte bitmap em dominio
publico que acompanha o repositorio -- veja assets/fonts/README.md.

Rode a partir da raiz do projeto:

    python tools/gen_assets.py
"""

from __future__ import annotations

import math
import random
import struct
import wave
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

RAIZ = Path(__file__).resolve().parent.parent
ASSETS = RAIZ / "assets"

# Fonte empacotada no proprio repositorio: gerar o atlas nao depende de nada
# instalado no sistema.
FONTE = ASSETS / "fonts" / "unscii-16.ttf"

# Espaco e tratado como avanco em branco, entao fica fora do atlas.
CHARSET = (
    "!\"#$%&'()*+,-./0123456789:;<=>?"
    "@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_"
    "`abcdefghijklmnopqrstuvwxyz{|}~"
    "ÀÁÂÃÇÉÊÍÓÔÕÚ"
    "àáâãçéêíóôõú"
    "°ªº→←↑↓"
)


def gerar_fonte(tamanho_px: int = 16, colunas: int = 16) -> None:
    if not FONTE.exists():
        raise SystemExit(f"fonte nao encontrada: {FONTE}")

    fonte = ImageFont.truetype(str(FONTE), tamanho_px)

    # A celula precisa caber o glifo mais alto e o descendente mais fundo de
    # todo o charset; caso contrario um "g" vaza para a celula de baixo no atlas.
    caixas = [fonte.getbbox(c) for c in CHARSET]
    topo = min(caixa[1] for caixa in caixas)
    base = max(caixa[3] for caixa in caixas)
    esquerda = min(caixa[0] for caixa in caixas)
    direita = max(caixa[2] for caixa in caixas)

    avanco = math.ceil(fonte.getlength("M"))
    largura_celula = max(avanco, direita - esquerda)
    altura_celula = base - topo

    linhas = math.ceil(len(CHARSET) / colunas)
    atlas = Image.new("RGBA", (colunas * largura_celula, linhas * altura_celula), (0, 0, 0, 0))
    desenho = ImageDraw.Draw(atlas)

    for indice, caractere in enumerate(CHARSET):
        cx = (indice % colunas) * largura_celula
        cy = (indice // colunas) * altura_celula
        desenho.text((cx - esquerda, cy - topo), caractere, font=fonte, fill=(255, 255, 255, 255))

    # A unscii e uma fonte de pixels: o antialiasing do rasterizador so borraria
    # as bordas, entao o alfa vira 1 bit.
    alfa = atlas.getchannel("A").point(lambda v: 255 if v >= 128 else 0)
    atlas.putalpha(alfa)

    destino_png = ASSETS / "fonts" / "mono.png"
    atlas.save(destino_png)

    destino_fnt = ASSETS / "fonts" / "mono.fnt"
    destino_fnt.write_text(
        "# fonte bitmap gerada por tools/gen_assets.py\n"
        f"cell {largura_celula} {altura_celula}\n"
        f"cols {colunas}\n"
        f"chars {CHARSET}\n",
        encoding="utf-8",
    )
    print(f"{destino_png.name}: {atlas.width}x{atlas.height} "
          f"(celula {largura_celula}x{altura_celula}, {len(CHARSET)} glifos)")


def desenhar_jogador(d, x0: int, lado: int, corpo_dy: int,
                     perna_esq_dy: int, perna_dir_dy: int) -> None:
    """Desenha um quadro do personagem na celula que comeca em x0.

    As pernas vao antes do corpo para a junta ficar escondida atras dele; o
    `corpo_dy` desce o tronco (e o visor e a antena junto), que e o que da o
    balanco tanto da respiracao quanto do passo.
    """
    contorno = (24, 44, 66, 255)
    # pernas: dy negativo levanta o pe (topo e base sobem juntos)
    for (px, dy) in ((x0 + 10, perna_esq_dy), (x0 + 17, perna_dir_dy)):
        d.rectangle([px, 20 + dy, px + 4, 29 + dy], fill=(52, 104, 148, 255), outline=contorno)
    # corpo
    d.rounded_rectangle([x0 + 4, 6 + corpo_dy, x0 + lado - 5, 21 + corpo_dy], radius=5,
                        fill=(86, 168, 220, 255), outline=contorno, width=2)
    # visor
    d.rectangle([x0 + 9, 11 + corpo_dy, x0 + lado - 10, 17 + corpo_dy],
                fill=(226, 244, 255, 255), outline=contorno)
    # pupilas
    d.rectangle([x0 + 11, 13 + corpo_dy, x0 + 13, 16 + corpo_dy], fill=contorno)
    d.rectangle([x0 + lado - 14, 13 + corpo_dy, x0 + lado - 12, 16 + corpo_dy], fill=contorno)
    # antena
    meio = x0 + lado // 2
    d.line([meio, 6 + corpo_dy, meio, 1 + corpo_dy], fill=contorno, width=2)
    d.ellipse([meio - 3, 0 + corpo_dy, meio + 1, 4 + corpo_dy], fill=(240, 196, 84, 255),
              outline=contorno)


def gerar_jogador(lado: int = 32, colunas: int = 4) -> None:
    """Folha de sprites do jogador: linha 0 parado, linha 1 andando.

    A grade e regular de proposito -- toda celula tem `lado` x `lado` --, porque
    e assim que jogo::Animacao recorta: a linha escolhe o clipe e a coluna
    escolhe o quadro, sem tabela de recortes por quadro.

    Cada quadro e (deslocamento do corpo, do pe esquerdo, do pe direito). Parado
    e so o tronco subindo e descendo um pixel; andando levanta um pe de cada vez
    e abaixa o corpo no meio do passo, quando o peso cai sobre a perna.
    """
    linhas = [
        [(0, 0, 0), (0, 0, 0), (1, 0, 0), (1, 0, 0)],       # parado (respiracao)
        [(0, -4, 0), (1, 0, 0), (0, 0, -4), (1, 0, 0)],     # andando
    ]

    img = Image.new("RGBA", (lado * colunas, lado * len(linhas)), (0, 0, 0, 0))
    for indice, quadros in enumerate(linhas):
        if len(quadros) != colunas:
            raise SystemExit("toda linha da folha precisa ter o mesmo numero de quadros")
        # Cada linha e desenhada em uma imagem de uma celula de altura e colada
        # na folha: assim os deslocamentos verticais dos quadros sao relativos a
        # celula, sem um offset de linha espalhado por cada primitiva.
        faixa = Image.new("RGBA", (lado * colunas, lado), (0, 0, 0, 0))
        d = ImageDraw.Draw(faixa)
        for coluna, (corpo, pe_esq, pe_dir) in enumerate(quadros):
            desenhar_jogador(d, coluna * lado, lado, corpo, pe_esq, pe_dir)
        img.paste(faixa, (0, indice * lado))

    destino = ASSETS / "textures" / "player.png"
    img.save(destino)
    print(f"{destino.name}: {img.width}x{img.height} "
          f"({len(linhas)} clipes de {colunas} quadros de {lado}px)")


def gerar_tiles(lado: int = 16) -> None:
    """Atlas 4x1: grama, terra, pedra, agua."""
    paletas = [
        ((96, 172, 84), (70, 138, 62), (52, 108, 48)),   # grama
        ((150, 110, 74), (122, 88, 58), (96, 68, 44)),   # terra
        ((132, 136, 148), (104, 108, 122), (78, 82, 96)),  # pedra
        ((72, 128, 200), (56, 106, 176), (44, 86, 150)),  # agua
    ]
    img = Image.new("RGBA", (lado * len(paletas), lado), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    for i, (base, medio, escuro) in enumerate(paletas):
        x0 = i * lado
        d.rectangle([x0, 0, x0 + lado - 1, lado - 1], fill=base)
        # ruido deterministico para o tile nao ficar chapado
        for y in range(lado):
            for x in range(lado):
                h = (x * 7 + y * 13 + i * 31) % 11
                if h == 0:
                    d.point((x0 + x, y), fill=medio)
                elif h == 1:
                    d.point((x0 + x, y), fill=escuro)
        d.rectangle([x0, 0, x0 + lado - 1, lado - 1], outline=escuro)
    destino = ASSETS / "textures" / "tiles.png"
    img.save(destino)
    print(f"{destino.name}: {img.width}x{img.height}")


def gerar_interior(lado: int = 16) -> None:
    """Atlas do interior da nave: 0 piso, 1 grade, 2 piso escuro, 3 parede,
    4 parede com faixa luminosa, 5 janela para o espaco."""
    largura = lado * 6
    img = Image.new("RGBA", (largura, lado), (0, 0, 0, 255))
    d = ImageDraw.Draw(img)

    def celula(i):
        return i * lado

    # 0: piso metalico com rebites
    x = celula(0)
    d.rectangle([x, 0, x + lado - 1, lado - 1], fill=(58, 66, 84))
    d.rectangle([x, 0, x + lado - 1, lado - 1], outline=(46, 53, 69))
    for py in (3, lado - 4):
        for px in (3, lado - 4):
            d.point((x + px, py), fill=(88, 98, 120))

    # 1: grade de ventilacao
    x = celula(1)
    d.rectangle([x, 0, x + lado - 1, lado - 1], fill=(44, 50, 66))
    for py in range(2, lado - 1, 3):
        d.line([x + 2, py, x + lado - 3, py], fill=(70, 80, 100))
    d.rectangle([x, 0, x + lado - 1, lado - 1], outline=(36, 42, 56))

    # 2: piso escuro (variacao)
    x = celula(2)
    d.rectangle([x, 0, x + lado - 1, lado - 1], fill=(50, 57, 73))
    d.rectangle([x, 0, x + lado - 1, lado - 1], outline=(42, 48, 62))
    d.line([x + 2, lado - 3, x + lado - 3, lado - 3], fill=(64, 72, 92))

    # 3: parede/casco
    x = celula(3)
    d.rectangle([x, 0, x + lado - 1, lado - 1], fill=(78, 86, 106))
    d.line([x, 0, x + lado - 1, 0], fill=(104, 114, 138))
    d.line([x + lado // 2, 1, x + lado // 2, lado - 1], fill=(64, 71, 90))
    d.line([x, lado - 1, x + lado - 1, lado - 1], fill=(52, 58, 74))

    # 4: parede com faixa luminosa
    x = celula(4)
    d.rectangle([x, 0, x + lado - 1, lado - 1], fill=(78, 86, 106))
    d.line([x, 0, x + lado - 1, 0], fill=(104, 114, 138))
    d.rectangle([x + 2, 5, x + lado - 3, 7], fill=(120, 220, 235))
    d.rectangle([x + 2, 8, x + lado - 3, 8], fill=(60, 130, 150))
    d.line([x, lado - 1, x + lado - 1, lado - 1], fill=(52, 58, 74))

    # 5: janela para o espaco
    x = celula(5)
    d.rectangle([x, 0, x + lado - 1, lado - 1], fill=(10, 12, 26))
    for (px, py, cor) in ((4, 4, (255, 255, 255)), (9, 7, (190, 210, 255)),
                          (6, 11, (255, 235, 190)), (12, 3, (160, 190, 255)),
                          (11, 12, (255, 255, 255))):
        d.point((x + px, py), fill=cor)
    d.rectangle([x, 0, x + lado - 1, lado - 1], outline=(96, 106, 130))

    destino = ASSETS / "textures" / "interior.png"
    img.save(destino)
    print(f"{destino.name}: {img.width}x{img.height} (6 tiles de {lado}px)")


def gerar_console(largura: int = 48, altura: int = 32) -> None:
    """Painel de pilotagem: e nele que o jogador aperta E."""
    img = Image.new("RGBA", (largura, altura), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)

    # corpo
    d.rounded_rectangle([0, 6, largura - 1, altura - 1], radius=3,
                        fill=(62, 70, 90), outline=(30, 35, 47), width=1)
    # tres telas
    telas = [((4, 9, 16, 20), (72, 200, 160)),
             ((18, 9, 30, 20), (110, 190, 255)),
             ((32, 9, 44, 20), (240, 180, 90))]
    for (caixa, cor) in telas:
        d.rectangle(caixa, fill=(14, 18, 30), outline=(24, 30, 44))
        x0, y0, x1, y1 = caixa
        for i, py in enumerate(range(y0 + 2, y1 - 1, 3)):
            largura_linha = (x1 - x0 - 4) - (i % 3) * 3
            d.line([x0 + 2, py, x0 + 2 + largura_linha, py], fill=cor)

    # teclado / borda inferior
    d.rectangle([3, 23, largura - 4, altura - 4], fill=(46, 53, 69), outline=(30, 35, 47))
    for px in range(6, largura - 6, 4):
        d.point((px, 26), fill=(120, 132, 156))
        d.point((px, 29), fill=(120, 132, 156))

    # antena/suporte no topo
    d.rectangle([largura // 2 - 8, 2, largura // 2 + 8, 6], fill=(52, 60, 78),
                outline=(30, 35, 47))

    destino = ASSETS / "textures" / "console.png"
    img.save(destino)
    print(f"{destino.name}: {img.width}x{img.height}")


def gerar_bancada(largura: int = 48, altura: int = 32) -> None:
    """Bancada de reparo: e nela que o jogador solda o casco de volta.

    Mesma pegada do console (3x2 tiles, encostada numa parede), mas em metal
    quente em vez de azul de instrumento: os dois moveis do conves precisam ser
    reconheciveis de longe, e a cor e o que faz isso antes do formato.
    """
    img = Image.new("RGBA", (largura, altura), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)

    # painel de ferramentas na parte alta do movel
    d.rectangle([2, 0, largura - 3, 11], fill=(52, 47, 42), outline=(28, 26, 24))
    for px in range(7, largura - 6, 7):
        d.line([px, 2, px, 7], fill=(148, 152, 162))
        d.point((px, 8), fill=(120, 110, 90))

    # tampo
    d.rectangle([0, 12, largura - 1, 17], fill=(96, 89, 76), outline=(30, 28, 26))
    # a chapa presa ao tampo, onde a solda acontece
    d.rectangle([4, 13, 13, 16], fill=(126, 96, 44), outline=(60, 48, 30))

    # corpo e gavetas
    d.rectangle([1, 18, largura - 2, altura - 1], fill=(64, 59, 52), outline=(30, 28, 26))
    for px in (12, 24, 36):
        d.line([px, 19, px, altura - 2], fill=(44, 41, 37))
    for gx in (6, 18, 30, 42):
        d.line([gx - 3, 24, gx + 3, 24], fill=(132, 127, 116))

    destino = ASSETS / "textures" / "bancada.png"
    img.save(destino)
    print(f"{destino.name}: {img.width}x{img.height}")


def gerar_wav(nome: str, freq: float, duracao: float, forma: str = "quadrada") -> None:
    taxa = 44100
    total = int(taxa * duracao)
    quadros = bytearray()
    for i in range(total):
        t = i / taxa
        fase = 2.0 * math.pi * freq * t
        if forma == "quadrada":
            amostra = 1.0 if math.sin(fase) >= 0.0 else -1.0
        else:
            amostra = math.sin(fase)
        # envelope de ataque/decaimento para nao estalar
        ataque = min(1.0, i / (taxa * 0.005))
        decaimento = max(0.0, 1.0 - i / total)
        valor = int(amostra * ataque * (decaimento ** 2) * 0.35 * 32767)
        quadros += struct.pack("<h", valor)

    destino = ASSETS / "audio" / nome
    with wave.open(str(destino), "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(taxa)
        w.writeframes(bytes(quadros))
    print(f"{destino.name}: {duracao * 1000:.0f} ms @ {freq:.0f} Hz")


def ruido_marrom(total: int, taxa: int, corte: float, semente: int) -> list[float]:
    """Ruido marrom (-6 dB por oitava) que emenda o fim no comeco.

    Ruido marrom e ruido branco integrado; um integrador com vazamento
    (y = a*y + (1-a)*x) da o mesmo espectro e ainda por cima nao deriva.

    O filtro roda em circulo: a primeira passada serve so para aquecer o estado
    e a segunda e a que fica. Assim a ultima amostra emenda na primeira
    exatamente como emendaria no meio do sinal -- sem crossfade, sem estalo na
    virada do loop. Funciona porque a resposta do filtro decai (a**total e
    desprezivel), entao o estado depois de uma volta ja e o estado de regime no
    ponto de partida.
    """
    aleatorio = random.Random(semente)
    branco = [aleatorio.uniform(-1.0, 1.0) for _ in range(total)]

    a = math.exp(-2.0 * math.pi * corte / taxa)
    estado = 0.0
    for x in branco:  # aquecimento: so o estado final interessa
        estado = a * estado + (1.0 - a) * x
    marrom = []
    for x in branco:
        estado = a * estado + (1.0 - a) * x
        marrom.append(estado)

    media = sum(marrom) / total
    return [v - media for v in marrom]


def escrever_wav(nome: str, amostras: list[float], taxa: int) -> None:
    quadros = bytearray()
    for v in amostras:
        quadros += struct.pack("<h", int(max(-0.99, min(0.99, v)) * 32767))
    destino = ASSETS / "audio" / nome
    with wave.open(str(destino), "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(taxa)
        w.writeframes(bytes(quadros))


def gerar_ambiente(nome: str, duracao: float = 4.0, taxa: int = 22050,
                   corte: float = 18.0, rms_alvo: float = 0.16,
                   semente: int = 0xB2011) -> None:
    """Ruido marrom em loop: o zumbido do lado de fora da nave."""
    total = int(taxa * duracao)
    marrom = ruido_marrom(total, taxa, corte, semente)

    # Normaliza por RMS, nao por pico: o pico de um ruido marrom e erratico e
    # deixaria o volume percebido a merce de um unico estouro.
    rms = math.sqrt(sum(v * v for v in marrom) / total)
    ganho = rms_alvo / rms if rms > 0.0 else 0.0
    escrever_wav(nome, [v * ganho for v in marrom], taxa)
    print(f"{nome}: {duracao:.1f} s @ {taxa} Hz em loop (ruido marrom)")


def gerar_impacto(nome: str, duracao: float = 0.5, taxa: int = 22050,
                  corte: float = 110.0, semente: int = 0x1A9A,
                  decaimento: float = 9.0) -> None:
    """Baque: o mesmo ruido marrom, so que com decaimento rapido. Corte mais
    alto que o do ambiente para o estouro ter corpo em vez de so rugir.

    O decaimento e o que separa um esbarrao no casco (rapido, seco) do estouro
    que acaba com a nave (grave, com cauda longa)."""
    total = int(taxa * duracao)
    marrom = ruido_marrom(total, taxa, corte, semente)

    pico = max(abs(v) for v in marrom) or 1.0
    amostras = []
    for i, v in enumerate(marrom):
        t = i / taxa
        ataque = min(1.0, t / 0.003)
        amostras.append(v / pico * 0.9 * ataque * math.exp(-t * decaimento))
    escrever_wav(nome, amostras, taxa)
    print(f"{nome}: {duracao * 1000:.0f} ms (baque de ruido marrom)")


def gerar_sirene(nome: str, duracao: float = 0.5, taxa: int = 22050,
                 freq: float = 620.0) -> None:
    """Tom continuo da sirene de casco critico, em loop.

    O que faz isto soar como uma sirene nao esta no arquivo: o WAV e so o
    timbre, e quem o faz ir e voltar e o `Flight`, abrindo e fechando o ganho
    desta voz uma vez por ciclo do alarme (veja Flight::alarme). A ida e a volta
    ficam do lado do jogo, e nao gravadas aqui, porque e o mesmo numero que
    acende a luz vermelha do conves -- gravada no WAV, ela andaria pelo relogio
    do dispositivo de audio e se separaria da luz em poucos minutos.

    O timbre e uma fundamental com a quinta e a oitava por cima: o corte
    metalico que um seno puro nao tem. Toda parcial precisa fechar um numero
    inteiro de ciclos na duracao, senao a onda nao emenda no fim do loop e a
    volta estala.
    """
    parciais = ((1.0, 1.0), (1.5, 0.45), (2.0, 0.25))
    for multiplo, _ in parciais:
        ciclos = freq * multiplo * duracao
        if abs(ciclos - round(ciclos)) > 1e-9:
            raise SystemExit(f"parcial {multiplo}x de {freq} Hz nao fecha o loop "
                             f"em {duracao} s ({ciclos} ciclos)")

    total = int(taxa * duracao)
    amostras = []
    for i in range(total):
        t = i / taxa
        amostras.append(sum(peso * math.sin(2.0 * math.pi * freq * multiplo * t)
                            for multiplo, peso in parciais))

    pico = max(abs(v) for v in amostras) or 1.0
    escrever_wav(nome, [v / pico * 0.7 for v in amostras], taxa)
    print(f"{nome}: {duracao * 1000:.0f} ms @ {freq:.0f} Hz em loop (tom do alarme)")


def main() -> None:
    for sub in ("textures", "fonts", "audio"):
        (ASSETS / sub).mkdir(parents=True, exist_ok=True)
    gerar_fonte()
    gerar_jogador()
    gerar_tiles()
    gerar_interior()
    gerar_console()
    gerar_bancada()
    gerar_wav("blip.wav", 660.0, 0.07)
    gerar_wav("confirm.wav", 990.0, 0.14)
    gerar_wav("back.wav", 330.0, 0.12)
    # A solda que nao pegou: mais grave e mais longa que o "voltar", porque na
    # bancada os dois sons acontecem lado a lado e precisam se distinguir.
    gerar_wav("falha.wav", 165.0, 0.22)
    gerar_ambiente("espaco.wav")
    gerar_sirene("sirene.wav")
    gerar_impacto("impacto.wav")
    # A faisca da solda e o mesmo baque com o corte bem mais alto e a cauda bem
    # mais curta: o ruido sai quase branco, que e o chiado agudo que se espera de
    # um ponto de solda em vez do estouro grave de uma rocha no casco.
    gerar_impacto("solda.wav", duracao=0.16, corte=900.0, semente=0x50DA,
                  decaimento=34.0)
    # O fim da nave: o mesmo baque, mais grave e com uma cauda que dura o
    # tempo de ver os destrocos se afastarem.
    gerar_impacto("destruicao.wav", duracao=2.2, corte=52.0, semente=0xDEAD,
                  decaimento=2.0)


if __name__ == "__main__":
    main()
