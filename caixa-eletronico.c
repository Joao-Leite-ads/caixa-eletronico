#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ARQUIVO "contas.dat"
#define ARQ_TRANSACAO "transacoes.dat"
#define ARQ_TEMP "contas_temp.dat"

typedef enum
{
    CONTA_ENCERRADA,
    CONTA_ATIVA
} SituacaoConta;

typedef struct
{
    long numero;
    char titular[100];
    char cpf[15];
    double saldo;
    SituacaoConta situacao;
} Conta;

typedef enum
{
    DEPOSITO,
    SAQUE,
    TRANSFERENCIA_ENVIADA,
    TRANSFERENCIA_RECEBIDA
} TipoTransacao;

typedef struct
{
    long conta;
    long contaRelacionada;
    TipoTransacao tipo;
    double valor;
} Transacao;

static void lerTexto(const char *msg, char *dest, int tam)
{
    printf("%s", msg);
    if (fgets(dest, tam, stdin) == NULL)
        exit(0);

    char *quebra = strchr(dest, '\n');
    if (quebra)
        *quebra = '\0';
    else
    {
        int c;
        while ((c = getchar()) != '\n' && c != EOF)
            ;
    }
}

static double lerNumero(const char *msg)
{
    char texto[50], *fim;
    double valor;

    while (1)
    {
        lerTexto(msg, texto, sizeof(texto));
        for (char *p = texto; *p; p++)
            if (*p == ',')
                *p = '.';

        valor = strtod(texto, &fim);
        if (fim != texto && *fim == '\0')
            return valor;

        printf("Valor inválido, tente de novo.\n");
    }
}

int buscarConta(FILE *arquivo, long numeroConta, Conta *conta)
{
    Conta lida;

    rewind(arquivo);
    while (fread(&lida, sizeof(Conta), 1, arquivo) == 1)
    {
        if (lida.numero == numeroConta)
        {
            *conta = lida;
            return 1;
        }
    }
    return 0;
}

int atualizarConta(FILE *arquivo, const Conta *conta)
{
    if (fseek(arquivo, -(long)sizeof(Conta), SEEK_CUR) != 0)
        return 0;
    if (fwrite(conta, sizeof(Conta), 1, arquivo) != 1)
        return 0;
    return fflush(arquivo) == 0;
}

static int contaAtiva(FILE *arquivo, long numero, Conta *conta)
{
    if (!buscarConta(arquivo, numero, conta))
    {
        printf("Conta %ld não encontrada.\n", numero);
        return 0;
    }
    if (conta->situacao != CONTA_ATIVA)
    {
        printf("A conta %ld está encerrada.\n", numero);
        return 0;
    }
    return 1;
}

static void registrarTransacao(long conta, long relacionada, TipoTransacao tipo, double valor)
{
    Transacao t = {conta, relacionada, tipo, valor};
    FILE *arquivo = fopen(ARQ_TRANSACAO, "ab");

    if (arquivo == NULL || fwrite(&t, sizeof(Transacao), 1, arquivo) != 1)
        printf("Aviso: não consegui gravar o histórico.\n");

    if (arquivo != NULL)
        fclose(arquivo);
}

static int movimentar(long numero, double valor, int saque)
{
    Conta conta;
    int ok = 0;

    if (valor <= 0)
    {
        printf("O valor precisa ser maior que zero.\n");
        return 0;
    }

    FILE *arquivo = fopen(ARQUIVO, "r+b");
    if (arquivo == NULL)
    {
        printf("Não foi possível abrir o arquivo de contas.\n");
        return 0;
    }

    if (contaAtiva(arquivo, numero, &conta))
    {
        if (saque && conta.saldo < valor)
            printf("Saldo insuficiente (R$ %.2f).\n", conta.saldo);
        else
        {
            conta.saldo += saque ? -valor : valor;
            ok = atualizarConta(arquivo, &conta);
            if (!ok)
                printf("Erro ao gravar o novo saldo.\n");
        }
    }

    fclose(arquivo);
    return ok;
}

int depositar(long numeroConta, double valor)
{
    return movimentar(numeroConta, valor, 0);
}

int sacar(long numeroConta, double valor)
{
    return movimentar(numeroConta, valor, 1);
}

int transferir(long contaOrigem, long contaDestino, double valor)
{
    Conta origem, destino;

    if (contaOrigem == contaDestino)
    {
        printf("Não dá pra transferir para a própria conta.\n");
        return 0;
    }
    if (valor <= 0)
    {
        printf("O valor precisa ser maior que zero.\n");
        return 0;
    }

    FILE *arquivo = fopen(ARQUIVO, "rb");
    if (arquivo == NULL)
    {
        printf("Não foi possível abrir o arquivo de contas.\n");
        return 0;
    }

    int ok = contaAtiva(arquivo, contaOrigem, &origem) && contaAtiva(arquivo, contaDestino, &destino);
    fclose(arquivo);

    if (!ok)
        return 0;
    if (origem.saldo < valor)
    {
        printf("Saldo insuficiente (R$ %.2f).\n", origem.saldo);
        return 0;
    }

    if (!sacar(contaOrigem, valor))
        return 0;

    if (!depositar(contaDestino, valor))
    {
        printf("Falha no depósito, devolvendo o valor à origem...\n");
        depositar(contaOrigem, valor);
        return 0;
    }

    registrarTransacao(contaOrigem, contaDestino, TRANSFERENCIA_ENVIADA, valor);
    registrarTransacao(contaDestino, contaOrigem, TRANSFERENCIA_RECEBIDA, valor);
    return 1;
}

void cadastrarConta(void)
{
    Conta nova, outra;

    nova.numero = (long)lerNumero("Número da conta: ");
    lerTexto("Nome do titular: ", nova.titular, sizeof(nova.titular));
    lerTexto("CPF: ", nova.cpf, sizeof(nova.cpf));

    if (nova.numero <= 0 || nova.titular[0] == '\0' || nova.cpf[0] == '\0')
    {
        printf("Dados inválidos: número, nome e CPF são obrigatórios.\n");
        return;
    }

    nova.saldo = 0.0;
    nova.situacao = CONTA_ATIVA;

    FILE *arquivo = fopen(ARQUIVO, "r+b");
    if (arquivo == NULL)
        arquivo = fopen(ARQUIVO, "w+b");
    if (arquivo == NULL)
    {
        printf("Erro ao abrir o arquivo de contas.\n");
        return;
    }

    if (buscarConta(arquivo, nova.numero, &outra))
        printf("Já existe uma conta com o número %ld.\n", nova.numero);
    else
    {
        fseek(arquivo, 0, SEEK_END);
        if (fwrite(&nova, sizeof(Conta), 1, arquivo) == 1)
            printf("Conta %ld cadastrada com sucesso!\n", nova.numero);
        else
            printf("Erro ao gravar a conta.\n");
    }

    fclose(arquivo);
}

void consultarConta(void)
{
    Conta conta;
    long numero = (long)lerNumero("Número da conta: ");

    FILE *arquivo = fopen(ARQUIVO, "rb");
    if (arquivo == NULL)
    {
        printf("Nenhuma conta cadastrada ainda.\n");
        return;
    }

    if (buscarConta(arquivo, numero, &conta))
        printf("\nConta:    %ld\nTitular:  %s\nCPF:      %s\nSaldo:    R$ %.2f\nSituação: %s\n",
               conta.numero, conta.titular, conta.cpf, conta.saldo,
               conta.situacao == CONTA_ATIVA ? "ATIVA" : "ENCERRADA");
    else
        printf("Conta %ld não encontrada.\n", numero);

    fclose(arquivo);
}

void listarContas(void)
{
    Conta conta;
    int total = 0;

    int filtro = (int)lerNumero("1 - Todas | 2 - Somente ativas | 3 - Somente encerradas: ");
    if (filtro < 1 || filtro > 3)
    {
        printf("Filtro inválido.\n");
        return;
    }

    FILE *arquivo = fopen(ARQUIVO, "rb");
    if (arquivo == NULL)
    {
        printf("Nenhuma conta cadastrada ainda.\n");
        return;
    }

    printf("\n%-8s %-30s %-15s %12s  %s\n", "Conta", "Titular", "CPF", "Saldo", "Situação");
    while (fread(&conta, sizeof(Conta), 1, arquivo) == 1)
    {
        if ((filtro == 2 && conta.situacao != CONTA_ATIVA) ||
            (filtro == 3 && conta.situacao != CONTA_ENCERRADA))
            continue;

        printf("%-8ld %-30.30s %-15s %12.2f  %s\n",
               conta.numero, conta.titular, conta.cpf, conta.saldo,
               conta.situacao == CONTA_ATIVA ? "ATIVA" : "ENCERRADA");
        total++;
    }

    if (total == 0)
        printf("Nenhuma conta encontrada.\n");

    fclose(arquivo);
}

int encerrarConta(long numeroConta)
{
    Conta conta;
    int ok = 0;

    FILE *arquivo = fopen(ARQUIVO, "r+b");
    if (arquivo == NULL)
    {
        printf("Não foi possível abrir o arquivo de contas.\n");
        return 0;
    }

    if (contaAtiva(arquivo, numeroConta, &conta))
    {
        if (conta.saldo > 0.005 || conta.saldo < -0.005)
            printf("Só é possível encerrar contas com saldo zero (R$ %.2f).\n", conta.saldo);
        else
        {
            conta.situacao = CONTA_ENCERRADA;
            ok = atualizarConta(arquivo, &conta);
            if (!ok)
                printf("Erro ao gravar a alteração.\n");
        }
    }

    fclose(arquivo);
    return ok;
}

void extrato(void)
{
    Conta conta;
    Transacao t;
    int total = 0;
    const char *nomes[] = {"Depósito", "Saque", "Transferência enviada", "Transferência recebida"};

    long numero = (long)lerNumero("Número da conta: ");

    FILE *contas = fopen(ARQUIVO, "rb");
    if (contas == NULL)
    {
        printf("Nenhuma conta cadastrada ainda.\n");
        return;
    }
    int existe = buscarConta(contas, numero, &conta);
    fclose(contas);

    if (!existe)
    {
        printf("Conta %ld não encontrada.\n", numero);
        return;
    }

    printf("\nExtrato da conta %ld - %s\n", conta.numero, conta.titular);

    FILE *arquivo = fopen(ARQ_TRANSACAO, "rb");
    if (arquivo != NULL)
    {
        while (fread(&t, sizeof(Transacao), 1, arquivo) == 1)
        {
            if (t.conta != numero)
                continue;

            printf("%-24s %c R$ %9.2f", nomes[t.tipo],
                   (t.tipo == DEPOSITO || t.tipo == TRANSFERENCIA_RECEBIDA) ? '+' : '-', t.valor);
            if (t.tipo == TRANSFERENCIA_ENVIADA || t.tipo == TRANSFERENCIA_RECEBIDA)
                printf("  (conta %ld)", t.contaRelacionada);
            printf("\n");
            total++;
        }
        fclose(arquivo);
    }

    if (total == 0)
        printf("Nenhuma movimentação encontrada.\n");
    printf("Saldo atual: R$ %.2f\n", conta.saldo);
}

int reativarConta(long numeroConta)
{
    Conta conta;
    int ok = 0;

    FILE *arquivo = fopen(ARQUIVO, "r+b");
    if (arquivo == NULL)
    {
        printf("Não foi possível abrir o arquivo de contas.\n");
        return 0;
    }

    if (!buscarConta(arquivo, numeroConta, &conta))
        printf("Conta %ld não encontrada.\n", numeroConta);
    else if (conta.situacao == CONTA_ATIVA)
        printf("Essa conta já está ativa.\n");
    else
    {
        conta.situacao = CONTA_ATIVA;
        ok = atualizarConta(arquivo, &conta);
        if (!ok)
            printf("Erro ao gravar a alteração.\n");
    }

    fclose(arquivo);
    return ok;
}

void removerContasEncerradas(void)
{
    Conta conta;
    int removidas = 0, erro = 0;
    char resposta[10];

    lerTexto("Isso apaga as contas encerradas para sempre. Continuar? (s/n): ",
             resposta, sizeof(resposta));
    if (resposta[0] != 's' && resposta[0] != 'S')
    {
        printf("Operação cancelada.\n");
        return;
    }

    FILE *original = fopen(ARQUIVO, "rb");
    if (original == NULL)
    {
        printf("Nenhuma conta cadastrada ainda.\n");
        return;
    }

    FILE *temporario = fopen(ARQ_TEMP, "wb");
    if (temporario == NULL)
    {
        printf("Não foi possível criar o arquivo temporário.\n");
        fclose(original);
        return;
    }

    while (!erro && fread(&conta, sizeof(Conta), 1, original) == 1)
    {
        if (conta.situacao == CONTA_ENCERRADA)
            removidas++;
        else if (fwrite(&conta, sizeof(Conta), 1, temporario) != 1)
            erro = 1;
    }

    fclose(original);
    fclose(temporario);

    if (erro)
    {
        remove(ARQ_TEMP);
        printf("Erro durante a cópia. Nada foi alterado.\n");
    }
    else if (remove(ARQUIVO) != 0 || rename(ARQ_TEMP, ARQUIVO) != 0)
        printf("Erro ao substituir o arquivo de contas.\n");
    else
        printf("%d conta(s) encerrada(s) removida(s).\n", removidas);
}

int main(void)
{
    int opcao;
    long numero, destino;
    double valor;

    do
    {
        printf("\n===== CAIXA ELETRÔNICO =====\n"
               "1 - Cadastrar conta\n2 - Consultar conta\n3 - Realizar depósito\n"
               "4 - Realizar saque\n5 - Realizar transferência\n6 - Listar contas\n"
               "7 - Encerrar conta\n8 - Extrato\n9 - Reativar conta\n"
               "10 - Remover contas encerradas (admin)\n0 - Sair\n");
        opcao = (int)lerNumero("Escolha uma opção: ");

        switch (opcao)
        {
        case 1:
            cadastrarConta();
            break;
        case 2:
            consultarConta();
            break;
        case 3:
            numero = (long)lerNumero("Número da conta: ");
            valor = lerNumero("Valor do depósito: R$ ");
            if (depositar(numero, valor))
            {
                registrarTransacao(numero, 0, DEPOSITO, valor);
                printf("Depósito realizado com sucesso!\n");
            }
            break;
        case 4:
            numero = (long)lerNumero("Número da conta: ");
            valor = lerNumero("Valor do saque: R$ ");
            if (sacar(numero, valor))
            {
                registrarTransacao(numero, 0, SAQUE, valor);
                printf("Saque realizado com sucesso!\n");
            }
            break;
        case 5:
            numero = (long)lerNumero("Conta de origem: ");
            destino = (long)lerNumero("Conta de destino: ");
            valor = lerNumero("Valor da transferência: R$ ");
            if (transferir(numero, destino, valor))
                printf("Transferência realizada com sucesso!\n");
            break;
        case 6:
            listarContas();
            break;
        case 7:
            numero = (long)lerNumero("Número da conta: ");
            if (encerrarConta(numero))
                printf("Conta encerrada com sucesso.\n");
            break;
        case 8:
            extrato();
            break;
        case 9:
            numero = (long)lerNumero("Número da conta: ");
            if (reativarConta(numero))
                printf("Conta reativada com sucesso.\n");
            break;
        case 10:
            removerContasEncerradas();
            break;
        case 0:
            printf("Até mais!\n");
            break;
        default:
            printf("Opção inválida.\n");
        }
    } while (opcao != 0);

    return 0;
}