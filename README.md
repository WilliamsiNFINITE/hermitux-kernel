# Compte rendu du TP de virtualisation

Ce rapport documente la manière dont j'ai réalisé le TP sur la virtualisation. 

L'objectif de ce TP est d'implémenter deux fonctions d'appel système dans l'unikernel [Hermitux](https://github.com/ssrg-vt/hermitux). 
Les deux fonctions à implémenter sont truncate et ftruncate. Pour plus d'information sur le TP, veuillez consulter le [sujet](https://olivierpierre.github.io/virt-101/lab-subject.pdf)

## Sommaire

* [Resume des modifications](#resume-des-modifications)
* [Implementation des appels systèmes ftruncate et truncate](#implementation-des-appels-systemes-ftruncate-et-truncate)
* [Hermitux Kernel](#hermitux-kernel)


## Resume des modifications

Ajout des paramètres pour désactiver les appels systèmes dans syscall-config.h.
Creation des fichierstruncate.c et ftruncate.c dans le repertoire.
les constructeur ont été ajouté dans le fichier syscall.h.
Ajout des case 'UHYVE_PORT_FTRUNCATE' et 'UHYVE_PORT_TRUNCATE' à implémenter dans uhyve.c.


## Implementation des appels systèmes ftruncate et truncate

J'ai consulté la page wiki [Syscall Based Modularity](https://github.com/ssrg-vt/hermitux/wiki/Syscall-Based-Modularity) qui m'a dirigé vers ce [fichier](https://github.com/ssrg-vt/hermitux-kernel/blob/master/include/hermit/syscall-config.h). On peut y voir une liste de syscall (ou appel système) qui peuvent être 'disable'. Pour commencer, je vais y ajouter les appels systèmes `ftruncate` et `truncate`.

```C
/* Uncomment lines in this file to exclude the implementation of the
 * corresponding syscall from the build */

//#define DISABLE_SYS_TRUNCATE
//#define DISABLE_SYS_FTRUNCATE
```

En fouillant un peu on trouve un fichier syscall.h (voir image suivante) dans lequel on trouve la déclaration d'une méthode `sys_read` qui permet d'avoir une piste de recherche. En effet, pour implémenter nos deux appels système, nous nous inspirerons de la manière dont est implémentée cet appel système. On ajoute donc notre déclaration des appels dans le fichier syscall.h et on  crée deux fichiers truncate.c et ftruncate.c 

![image](https://user-images.githubusercontent.com/91114817/211047053-f9532698-1d9f-4b0f-8ad1-f6a505c22056.png)

Avec les conseils de l'encadrant, je suis allé regarder plutôt le fichier `sys_creat`
Il reste à determiner comment implémenter les handler ainsi que le numéro des appels systèmes pour truncate et ftruncate. 

J'ai également jeté un oeil à uhyve_send, utilisé dans sys_creat et que nous allons réutilisé pour nos deux appels systèmes.
On remarque notamment que lors de l'usage de cette méthode, on doit utiliser la méthode virt_to_phys pour faire correspondre l'adresse virtuelle avec l'adresse physique lors du passage de l'argument path. 

Les case 'UHYVE_PORT_FTRUNCATE' et 'UHYVE_PORT_TRUNCATE' ont été créer afin de les passer en paramètre de la méthode uhyve_send. 

Il reste à trouver les handler ainsi qu'à détermilner quels sont les id des appels systèmes ftruncate et truncate.


## Hermitux Kernel

This repository contains the sources for the kernel of HermiTux, a unikernel
that is binary compatible with Linux applications. To try HermiTux please
follow this link:

https://github.com/ssrg-vt/hermitux
