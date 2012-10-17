#include <QApplication>
#include <QFont>
#include <QPushButton>
#include <QLineEdit>
#include <QWidget>
#include <QVBoxLayout>
#include <QMouseEvent>
#include <QComboBox>

#include <QFileInfo>
#include <QDir>

#include <mono/jit/jit.h>
#include <mono/metadata/assembly.h>

#include <string>
#include <vector>

typedef std::pair<QString, int> op_pair;
typedef std::vector<op_pair> op_vector;

class MonoEmbedHelper
{
public:
	MonoEmbedHelper()
	{
		QString path = QFileInfo( QCoreApplication::applicationFilePath() ).absolutePath();
	#ifdef WIN32
		path += "/../";
	#else
		path += "/../../../";
	#endif
		std::string file (path.toUtf8().constData());
		MonoDomain* domain = mono_jit_init (file.c_str());
		
		QString core("Core.dll");
		QDir dir(path);
		QStringList filters;
		filters.append("*.dll");
		QStringList list = dir.entryList(filters);
		list.removeAll(core);

		QString corePath(path);
		corePath += "Core.dll";
		LoadCore(corePath);
		ProcessAssemblies(path, list);
	}

	const op_vector& GetOperations()
	{
		return _operations;
	}

	double ExecuteOperation(int gchandle, double a, double b)
	{
		void* args[2];
		args[0] = &a;
		args[1] = &b;
		MonoObject* object = mono_gchandle_get_target (gchandle);
		MonoMethod* virtualExecute = mono_object_get_virtual_method (object, _executeMethod);
		MonoObject* boxedResult = mono_runtime_invoke(virtualExecute, object, args, NULL);
		double d = *(double*)mono_object_unbox (boxedResult);

		return d;
	}

private:

	void ProcessAssemblies(const QString& path, const QStringList& assemblies)
	{
		for (int i = 0; i < assemblies.count(); i++)
		{
			QString filename = path;
			filename += assemblies.at(i);
			LoadOperation(filename);
		}
	}

	void LoadCore(QString path)
	{
		std::string file (path.toUtf8().constData());
		MonoAssembly *assembly = mono_domain_assembly_open (mono_domain_get(), file.c_str());
		MonoImage* image = mono_assembly_get_image (assembly);

		_operationItfClass = mono_class_from_name (image, "EmbedSample", "IOperation");
		_executeMethod = mono_class_get_method_from_name (_operationItfClass, "Execute", 2);
	}
	
	void LoadOperation(QString path)
	{
		std::string file (path.toUtf8().constData());
		MonoAssembly *assembly = mono_domain_assembly_open (mono_domain_get(), file.c_str());
		MonoImage* image = mono_assembly_get_image (assembly);

		MonoClass* klass = mono_class_from_name (image, "EmbedSample", "Operation");
		MonoObject* object = mono_object_new(mono_domain_get(), klass);
		MonoProperty* prop = mono_class_get_property_from_name (klass, "Name");
		MonoObject* propValue = mono_property_get_value (prop, object, NULL, NULL);
		QString name((QChar*)mono_string_chars ((MonoString*)propValue));
		_operations.push_back(op_pair(name, mono_gchandle_new (object, FALSE)));
	}

	std::vector<op_pair> _operations;
	MonoClass* _operationItfClass;
	MonoMethod* _executeMethod;
};

class MyDropDown : public QComboBox
{
public:
	MyDropDown(MonoEmbedHelper* embedHelper) : 
		_embedHelper(embedHelper)
	{
		for (int i = 0; i < _embedHelper->GetOperations().size(); i++)
		{
			op_pair operation = _embedHelper->GetOperations().at(i);
			this->addItem(operation.first, operation.second);
		}
	}

private:
	MonoEmbedHelper* _embedHelper;
};

class Adder : public QPushButton
{
  public:
    Adder(MonoEmbedHelper* embedHelper, MyDropDown* dropDown, QLineEdit* lineEdit1, QLineEdit* lineEdit2, QLineEdit* lineEdit3, QWidget *parent = 0) : 
    	QPushButton(parent), 
		_embedHelper(embedHelper),
		_dropDown(dropDown),
    	_lineEdit1(lineEdit1),
    	_lineEdit2(lineEdit2),
    	_lineEdit3(lineEdit3)
    {
    	this->setText("Add");
    }

  protected:
  	void mouseReleaseEvent ( QMouseEvent * event )
	{
		if(event->button() == Qt::LeftButton)
		{
			QString addend1 = _lineEdit1->text();
			QString addend2 = _lineEdit2->text();
			double d1 = addend1.toDouble();
			double d2 = addend2.toDouble();

			int index = _dropDown->currentIndex();
			int gchandle = _dropDown->itemData(index).toInt();
			double d = _embedHelper->ExecuteOperation(gchandle, d1, d2);

			QString sum = QString::number(d);
			_lineEdit3->setText(sum);

		} 
		QPushButton::mouseReleaseEvent(event);
	} 

private:
	MonoEmbedHelper* _embedHelper;
	MyDropDown* _dropDown;
	QLineEdit* _lineEdit1;
	QLineEdit* _lineEdit2;
	QLineEdit* _lineEdit3;
};

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
	MonoEmbedHelper embedHelper;

    QWidget window;
    window.resize(200, 120);

    QLineEdit lineEdit;
    QLineEdit lineEdit2;
    QLineEdit lineEdit3;
    lineEdit3.setReadOnly(true);
	
	MyDropDown dropDown(&embedHelper);

    Adder add(&embedHelper, &dropDown, &lineEdit, &lineEdit2, &lineEdit3);
    add.setFont(QFont("Times", 18, QFont::Bold));
	add.setGeometry(10, 40, 180, 40);


    QVBoxLayout *layout = new QVBoxLayout;
    layout->addWidget(&lineEdit);
    layout->addWidget(&lineEdit2);
    layout->addWidget(&dropDown);
    layout->addWidget(&add);
    layout->addWidget(&lineEdit3);
    window.setLayout(layout);


    window.show();
    return app.exec();
}
