# Amazon AlexaアプリによるGlimmerGoの設定例

1. `Amazon Alexa` アプリをスマホ等にインストールし、起動する。
2. `デバイス`メニューを選択し、Matterデバイスの接続を行う。
    |1 |2 |3 |
    |----|----|----|
    |![](./images/AlexaApp00001.PNG) | ![](./images/AlexaApp00002.PNG) | ![](./images/AlexaApp00003.PNG)|
    |4 |5 |6 |
    |![](./images/AlexaApp00004.PNG) | ![](./images/AlexaApp00005.PNG) | ![](./images/AlexaApp00006.PNG)|
3. LCDパネルに表示されている11桁のペアリングコードを入力する。  
    |1 |2 |3 はいを選択する|
    |----|----|----|
    |![](./images/AlexaApp00007.PNG) | ![](./images/AlexaApp00008.PNG) | ![](./images/AlexaApp00009.PNG)|
    |4 |5 デバイスが登録できた| |
    |![](./images/AlexaApp00010.PNG) | ![](./images/AlexaApp00011.PNG) | |
4. Flash Lightの設定
    |1 |2 |3 環境に合わせて設定する|
    |----|----|----|
    |![](./images/AlexaApp00013.PNG) | ![](./images/AlexaApp00014.PNG) | ![](./images/AlexaApp00015.PNG)|
    |4 |5 |  |
    |![](./images/AlexaApp00016.PNG) | ![](./images/AlexaApp00017.PNG) | |
5. 同様にOK Buttonの設定を行う。
6. 同様にNG Buttonの設定を行う。
7. 設定が完了すると、以下が表示される。
    ||
    |----|
    |![](./images/AlexaApp00018.PNG)|
8. `GlimmerGO`を選択すると、Deviceの概要が表示される。
    ||
    |----|
    |![](./images/AlexaApp00019.PNG)|
9.  `Flash Light`を選択すると、ON/OFFと明るさの設定が行える。これを使ってFlash Lightの点灯、およびMessageのテストが可能。
    ||
    |----|
    |![](./images/AlexaApp00020.PNG)|
10. OKボタンオン、OKボタンオフ時の定型アクションを作成する。  
    スマート電球(ResponseLight)を**緑**に点灯させる定型アクション、および消灯させる定型アクションはあらかじめ作っておき、ALEXAのアクションに紐づける。
    |1 OKボタンオン|2 |3 |
    |----|----|----|
    | ![](./images/AlexaApp00022.PNG) | ![](./images/AlexaApp00024.PNG)| ![](./images/AlexaApp00025.PNG)|
    |1 OKボタンオフ|2 |3 |
    |![](./images/AlexaApp00023.PNG) | `開いている時`で設定 | `ResponseLight`は`電源:オフ`にする|
11. NGボタンオン、NGボタンオフ時も同様の定型アクションを作成する。  
    スマート電球(ResponseLight)を**赤**に点灯させる定型アクション、および消灯させる定型アクションはあらかじめ作っておき、ALEXAのアクションに紐づける。
12. Flsh Light起動用の定型アクションを作成する。  
    これで、「夕食の時間だ」で音声トリガがかかり、Flash Lightが点灯、LCDパネルに「ｺﾞﾊﾝﾀﾞﾖ」が表示されるようになる。
    |1 |2 |3 |
    |----|----|----|
    | ![](./images/AlexaApp00026.PNG) | ![](./images/AlexaApp00027.PNG) | ![](./images/AlexaApp00028.PNG)|
    |4 |5 |6 |
    | ![](./images/AlexaApp00029.PNG) | ![](./images/AlexaApp00031.PNG) | ![](./images/AlexaApp00032.PNG)|

## スマート電球について
スマート電球は、Beamtecの([LDA-8WRGB-VOCE](https://ec.beamtec.co.jp/products/lda-8wrgb-voce))を使用している。